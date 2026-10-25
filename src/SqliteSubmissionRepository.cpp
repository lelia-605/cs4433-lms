#include "lms/data/SqliteSubmissionRepository.hpp"

namespace lms {

namespace {
const char* kSelectSubmission =
    "SELECT submission_id, assignment_id, student_id, state, submitted_at, days_late, "
    "raw_mark, feedback, marked_by, moderated_by, released_at FROM submission ";

Submission readSubmission(Statement& st) {
  Submission s;
  s.id = st.columnInt(0);
  s.assignmentId = st.columnInt(1);
  s.studentId = st.columnInt(2);
  s.state = stateFromString(st.columnText(3));
  s.submittedAt = fromIso(st.columnText(4));
  s.daysLate = st.columnInt(5);
  if (!st.columnIsNull(6)) s.rawMark = st.columnInt(6);
  s.feedback = st.columnText(7);
  if (!st.columnIsNull(8)) s.markedBy = st.columnInt(8);
  if (!st.columnIsNull(9)) s.moderatedBy = st.columnInt(9);
  if (!st.columnIsNull(10)) s.releasedAt = fromIso(st.columnText(10));
  return s;
}
}  // namespace

std::optional<Submission> SqliteSubmissionRepository::find(int assignmentId, int studentId) const {
  auto st = db_.prepare(std::string(kSelectSubmission) + "WHERE assignment_id = ? AND student_id = ?");
  st.bind(1, assignmentId);
  st.bind(2, studentId);
  if (!st.step()) return std::nullopt;
  return readSubmission(st);
}

int SqliteSubmissionRepository::upsertSubmission(const Submission& s) {
  auto st = db_.prepare(
      "INSERT INTO submission (assignment_id, student_id, state, submitted_at, days_late) "
      "VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT(assignment_id, student_id) DO UPDATE SET "
      "state = excluded.state, submitted_at = excluded.submitted_at, days_late = excluded.days_late");
  st.bind(1, s.assignmentId);
  st.bind(2, s.studentId);
  st.bind(3, stateToString(s.state));
  st.bind(4, toIso(s.submittedAt));
  st.bind(5, s.daysLate);
  st.step();
  auto id = db_.prepare("SELECT submission_id FROM submission WHERE assignment_id = ? AND student_id = ?");
  id.bind(1, s.assignmentId);
  id.bind(2, s.studentId);
  id.step();
  return id.columnInt(0);
}

int SqliteSubmissionRepository::recordVersion(const Submission& s, const std::string& content) {
  try {
    Transaction tx(db_);  // the submission row and its version are saved together or not at all
    const int submissionId = upsertSubmission(s);
    auto next = db_.prepare(
        "SELECT COALESCE(MAX(version_no), 0) + 1 FROM submission_version WHERE submission_id = ?");
    next.bind(1, submissionId);
    next.step();
    const int versionNo = next.columnInt(0);
    auto st = db_.prepare(
        "INSERT INTO submission_version (submission_id, version_no, content, submitted_at) "
        "VALUES (?, ?, ?, ?)");
    st.bind(1, submissionId);
    st.bind(2, versionNo);
    st.bind(3, content);
    st.bind(4, toIso(s.submittedAt));
    st.step();
    tx.commit();
    return versionNo;
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string("cannot save submission: ") + e.what());
  }
}

std::vector<SubmissionVersion> SqliteSubmissionRepository::versions(int submissionId) const {
  auto st = db_.prepare(
      "SELECT version_no, content, submitted_at FROM submission_version "
      "WHERE submission_id = ? ORDER BY version_no");
  st.bind(1, submissionId);
  std::vector<SubmissionVersion> out;
  while (st.step()) out.push_back({st.columnInt(0), st.columnText(1), fromIso(st.columnText(2))});
  return out;
}

std::vector<Submission> SqliteSubmissionRepository::findByAssignment(int assignmentId) const {
  auto st = db_.prepare(std::string(kSelectSubmission) + "WHERE assignment_id = ? ORDER BY student_id");
  st.bind(1, assignmentId);
  std::vector<Submission> out;
  while (st.step()) out.push_back(readSubmission(st));
  return out;
}

void SqliteSubmissionRepository::saveMark(int submissionId, int rawMark, const std::string& feedback,
                                          const AuditEntry& entry) {
  try {
    Transaction tx(db_);  // the mark and its audit entry are saved together or not at all
    auto st = db_.prepare(
        "UPDATE submission SET raw_mark = ?, feedback = ?, marked_by = ?, state = 'MARKED' "
        "WHERE submission_id = ?");
    st.bind(1, rawMark);
    st.bind(2, feedback);
    st.bind(3, entry.changedBy);
    st.bind(4, submissionId);
    st.step();
    if (db_.changes() == 0) throw NotFound("submission not found");
    appendAudit(entry);
    tx.commit();
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string("cannot save mark: ") + e.what());
  }
}

std::vector<AuditEntry> SqliteSubmissionRepository::auditTrail(int submissionId) const {
  auto st = db_.prepare(
      "SELECT audit_id, submission_id, changed_by, changed_at, old_value, new_value, reason "
      "FROM audit_log WHERE submission_id = ? ORDER BY audit_id");
  st.bind(1, submissionId);
  std::vector<AuditEntry> out;
  while (st.step()) {
    AuditEntry e;
    e.id = st.columnInt(0);
    e.submissionId = st.columnInt(1);
    e.changedBy = st.columnInt(2);
    e.changedAt = fromIso(st.columnText(3));
    if (!st.columnIsNull(4)) e.oldValue = st.columnInt(4);
    if (!st.columnIsNull(5)) e.newValue = st.columnInt(5);
    e.reason = st.columnText(6);
    out.push_back(e);
  }
  return out;
}

void SqliteSubmissionRepository::appendAudit(const AuditEntry& entry) {
  auto log = db_.prepare(
      "INSERT INTO audit_log (submission_id, changed_by, changed_at, old_value, new_value, reason) "
      "VALUES (?, ?, ?, ?, ?, ?)");
  log.bind(1, entry.submissionId);
  log.bind(2, entry.changedBy);
  log.bind(3, toIso(entry.changedAt));
  if (entry.oldValue) log.bind(4, *entry.oldValue); else log.bindNull(4);
  if (entry.newValue) log.bind(5, *entry.newValue); else log.bindNull(5);
  log.bind(6, entry.reason);
  log.step();
}

std::optional<Submission> SqliteSubmissionRepository::findById(int submissionId) const {
  auto st = db_.prepare(std::string(kSelectSubmission) + "WHERE submission_id = ?");
  st.bind(1, submissionId);
  if (!st.step()) return std::nullopt;
  return readSubmission(st);
}

std::vector<Submission> SqliteSubmissionRepository::awaitingModeration(int departmentId) const {
  auto st = db_.prepare(
      "SELECT s.submission_id, s.assignment_id, s.student_id, s.state, s.submitted_at, s.days_late, "
      "s.raw_mark, s.feedback, s.marked_by, s.moderated_by, s.released_at FROM submission s "
      "JOIN assignment a ON a.assignment_id = s.assignment_id "
      "JOIN course c ON c.course_id = a.course_id "
      "WHERE s.state = 'MARKED' AND c.department_id = ? ORDER BY s.submission_id");
  st.bind(1, departmentId);
  std::vector<Submission> out;
  while (st.step()) out.push_back(readSubmission(st));
  return out;
}

void SqliteSubmissionRepository::saveModeration(int submissionId, int rawMark, const AuditEntry& entry) {
  try {
    Transaction tx(db_);  // the moderation and its audit entry are saved together or not at all
    auto st = db_.prepare(
        "UPDATE submission SET raw_mark = ?, moderated_by = ?, state = 'MODERATED' "
        "WHERE submission_id = ? AND state = 'MARKED'");
    st.bind(1, rawMark);
    st.bind(2, entry.changedBy);
    st.bind(3, submissionId);
    st.step();
    if (db_.changes() == 0) throw NotFound("no marked submission with that id");
    appendAudit(entry);
    tx.commit();
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string("cannot save moderation: ") + e.what());
  }
}

int SqliteSubmissionRepository::releaseModerated(int assignmentId, TimePoint at) {
  auto st = db_.prepare(
      "UPDATE submission SET state = 'RELEASED', released_at = ? "
      "WHERE assignment_id = ? AND state = 'MODERATED'");
  st.bind(1, toIso(at));
  st.bind(2, assignmentId);
  st.step();
  return db_.changes();
}

}  // namespace lms

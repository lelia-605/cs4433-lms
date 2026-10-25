#include "lms/data/SqliteSubmissionRepository.hpp"

namespace lms {

namespace {
const char* kSelectSubmission =
    "SELECT submission_id, assignment_id, student_id, state, submitted_at, days_late FROM submission ";

Submission readSubmission(Statement& st) {
  Submission s;
  s.id = st.columnInt(0);
  s.assignmentId = st.columnInt(1);
  s.studentId = st.columnInt(2);
  s.state = stateFromString(st.columnText(3));
  s.submittedAt = fromIso(st.columnText(4));
  s.daysLate = st.columnInt(5);
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

}  // namespace lms

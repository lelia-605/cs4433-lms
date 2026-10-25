#include "lms/data/SqliteAssignmentRepository.hpp"

namespace lms {

namespace {
Assignment readAssignment(Statement& st) {
  Assignment a;
  a.id = st.columnInt(0);
  a.courseId = st.columnInt(1);
  a.title = st.columnText(2);
  a.deadline = fromIso(st.columnText(3));
  a.maxMark = st.columnInt(4);
  return a;
}
const char* kSelectAssignment = "SELECT assignment_id, course_id, title, deadline, max_mark FROM assignment ";
}  // namespace

int SqliteAssignmentRepository::addAssignment(const Assignment& a) {
  auto st = db_.prepare("INSERT INTO assignment (course_id, title, deadline, max_mark) VALUES (?, ?, ?, ?)");
  st.bind(1, a.courseId);
  st.bind(2, a.title);
  st.bind(3, toIso(a.deadline));
  st.bind(4, a.maxMark);
  try {
    st.step();
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string("cannot add assignment: ") + e.what());
  }
  return db_.lastInsertId();
}

std::optional<Assignment> SqliteAssignmentRepository::findAssignment(int id) const {
  auto st = db_.prepare(std::string(kSelectAssignment) + "WHERE assignment_id = ?");
  st.bind(1, id);
  if (!st.step()) return std::nullopt;
  return readAssignment(st);
}

std::vector<Assignment> SqliteAssignmentRepository::findByCourse(int courseId) const {
  auto st = db_.prepare(std::string(kSelectAssignment) + "WHERE course_id = ? ORDER BY deadline, assignment_id");
  st.bind(1, courseId);
  std::vector<Assignment> out;
  while (st.step()) out.push_back(readAssignment(st));
  return out;
}

void SqliteAssignmentRepository::saveExtension(const Extension& e) {
  auto st = db_.prepare(
      "INSERT INTO extension (assignment_id, student_id, new_deadline, granted_by, granted_at, reason) "
      "VALUES (?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(assignment_id, student_id) DO UPDATE SET "
      "new_deadline = excluded.new_deadline, granted_by = excluded.granted_by, "
      "granted_at = excluded.granted_at, reason = excluded.reason");
  st.bind(1, e.assignmentId);
  st.bind(2, e.studentId);
  st.bind(3, toIso(e.newDeadline));
  st.bind(4, e.grantedBy);
  st.bind(5, toIso(e.grantedAt));
  st.bind(6, e.reason);
  try {
    st.step();
  } catch (const DatabaseError& ex) {
    throw RuleViolation(std::string("cannot save extension: ") + ex.what());
  }
}

std::optional<Extension> SqliteAssignmentRepository::findExtension(int assignmentId, int studentId) const {
  auto st = db_.prepare(
      "SELECT extension_id, assignment_id, student_id, new_deadline, granted_by, granted_at, reason "
      "FROM extension WHERE assignment_id = ? AND student_id = ?");
  st.bind(1, assignmentId);
  st.bind(2, studentId);
  if (!st.step()) return std::nullopt;
  Extension e;
  e.id = st.columnInt(0);
  e.assignmentId = st.columnInt(1);
  e.studentId = st.columnInt(2);
  e.newDeadline = fromIso(st.columnText(3));
  e.grantedBy = st.columnInt(4);
  e.grantedAt = fromIso(st.columnText(5));
  e.reason = st.columnText(6);
  return e;
}

}  // namespace lms

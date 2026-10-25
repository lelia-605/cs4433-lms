#include "lms/data/SqliteCourseRepository.hpp"

namespace lms {

namespace {
// Constraint failures (duplicate code, missing department...) become RuleViolation.
void runInsertOrUpdate(Statement& st, const char* what) {
  try {
    st.step();
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string(what) + ": " + e.what());
  }
}
}  // namespace

int SqliteCourseRepository::addDepartment(const std::string& name) {
  auto st = db_.prepare("INSERT INTO department (name) VALUES (?)");
  st.bind(1, name);
  runInsertOrUpdate(st, "cannot add department");
  return db_.lastInsertId();
}

std::optional<Department> SqliteCourseRepository::findDepartment(int id) const {
  auto st = db_.prepare("SELECT department_id, name FROM department WHERE department_id = ?");
  st.bind(1, id);
  if (!st.step()) return std::nullopt;
  return Department{st.columnInt(0), st.columnText(1)};
}

int SqliteCourseRepository::addCourse(const Course& c) {
  auto st = db_.prepare("INSERT INTO course (code, name, department_id, lecturer_id) VALUES (?, ?, ?, ?)");
  st.bind(1, c.code);
  st.bind(2, c.name);
  st.bind(3, c.departmentId);
  if (c.lecturerId) st.bind(4, *c.lecturerId); else st.bindNull(4);
  runInsertOrUpdate(st, "cannot add course");
  return db_.lastInsertId();
}

std::optional<Course> SqliteCourseRepository::findCourse(int id) const {
  auto st = db_.prepare("SELECT course_id, code, name, department_id, lecturer_id FROM course WHERE course_id = ?");
  st.bind(1, id);
  if (!st.step()) return std::nullopt;
  Course c;
  c.id = st.columnInt(0);
  c.code = st.columnText(1);
  c.name = st.columnText(2);
  c.departmentId = st.columnInt(3);
  if (!st.columnIsNull(4)) c.lecturerId = st.columnInt(4);
  return c;
}

void SqliteCourseRepository::renameCourse(int id, const std::string& name) {
  auto st = db_.prepare("UPDATE course SET name = ? WHERE course_id = ?");
  st.bind(1, name);
  st.bind(2, id);
  runInsertOrUpdate(st, "cannot rename course");
  if (db_.changes() == 0) throw NotFound("course not found");
}

void SqliteCourseRepository::setLecturer(int courseId, int lecturerId) {
  auto st = db_.prepare("UPDATE course SET lecturer_id = ? WHERE course_id = ?");
  st.bind(1, lecturerId);
  st.bind(2, courseId);
  runInsertOrUpdate(st, "cannot assign lecturer");
  if (db_.changes() == 0) throw NotFound("course not found");
}

void SqliteCourseRepository::enrol(int studentId, int courseId) {
  auto st = db_.prepare("INSERT INTO enrolment (student_id, course_id) VALUES (?, ?)");
  st.bind(1, studentId);
  st.bind(2, courseId);
  runInsertOrUpdate(st, "cannot enrol student (already enrolled?)");
}

bool SqliteCourseRepository::isEnrolled(int studentId, int courseId) const {
  auto st = db_.prepare("SELECT 1 FROM enrolment WHERE student_id = ? AND course_id = ?");
  st.bind(1, studentId);
  st.bind(2, courseId);
  return st.step();
}

namespace {
Course readCourse(Statement& st) {
  Course c;
  c.id = st.columnInt(0);
  c.code = st.columnText(1);
  c.name = st.columnText(2);
  c.departmentId = st.columnInt(3);
  if (!st.columnIsNull(4)) c.lecturerId = st.columnInt(4);
  return c;
}
}  // namespace

std::vector<Course> SqliteCourseRepository::coursesForStudent(int studentId) const {
  auto st = db_.prepare(
      "SELECT c.course_id, c.code, c.name, c.department_id, c.lecturer_id FROM course c "
      "JOIN enrolment e ON e.course_id = c.course_id WHERE e.student_id = ? ORDER BY c.code");
  st.bind(1, studentId);
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}

std::vector<Course> SqliteCourseRepository::coursesForLecturer(int lecturerId) const {
  auto st = db_.prepare(
      "SELECT course_id, code, name, department_id, lecturer_id FROM course WHERE lecturer_id = ? ORDER BY code");
  st.bind(1, lecturerId);
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}

std::vector<int> SqliteCourseRepository::enrolledStudents(int courseId) const {
  auto st = db_.prepare("SELECT student_id FROM enrolment WHERE course_id = ? ORDER BY student_id");
  st.bind(1, courseId);
  std::vector<int> out;
  while (st.step()) out.push_back(st.columnInt(0));
  return out;
}

}  // namespace lms

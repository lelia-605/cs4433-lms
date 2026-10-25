#include "lms/services/AdminService.hpp"

#include <algorithm>
#include <cctype>

namespace lms {

void AdminService::require(const User& actor, Action action) {
  if (!actor.can(action)) throw AccessDenied("you are not allowed to do this");
}

void AdminService::requireText(const std::string& value, const char* field, std::size_t maxLength) {
  const bool blank = std::all_of(value.begin(), value.end(),
                                 [](unsigned char c) { return std::isspace(c) != 0; });
  if (blank) throw RuleViolation(std::string(field) + " must not be empty");
  if (value.size() > maxLength)
    throw RuleViolation(std::string(field) + " is too long (maximum " + std::to_string(maxLength) + ")");
}

void AdminService::requirePassword(const std::string& password) const {
  if (password.size() < kMinPasswordLength)
    throw RuleViolation("password must have at least " + std::to_string(kMinPasswordLength) + " characters");
}

int AdminService::createUser(const User& actor, const std::string& username,
                             const std::string& password, const std::string& fullName, Role role,
                             std::optional<int> departmentId) {
  require(actor, Action::ManageUsers);
  requireText(username, "username", 50);
  if (std::any_of(username.begin(), username.end(), [](unsigned char c) { return std::isspace(c) != 0; }))
    throw RuleViolation("username must not contain spaces");
  requireText(fullName, "full name", 100);
  requirePassword(password);
  const bool needsDepartment = role == Role::Lecturer || role == Role::HeadOfDepartment;
  if (needsDepartment && !departmentId) throw RuleViolation("a lecturer or head of department needs a department");
  if (departmentId && !courses_.findDepartment(*departmentId)) throw NotFound("department not found");
  return users_.add({username, hasher_.hash(password), fullName, role, departmentId});
}

void AdminService::editUser(const User& actor, int userId, const std::string& fullName,
                            std::optional<int> departmentId) {
  require(actor, Action::ManageUsers);
  requireText(fullName, "full name", 100);
  if (departmentId && !courses_.findDepartment(*departmentId)) throw NotFound("department not found");
  users_.updateProfile(userId, fullName, departmentId);
}

void AdminService::deactivateUser(const User& actor, int userId) {
  require(actor, Action::ManageUsers);
  if (actor.id() == userId) throw RuleViolation("you cannot deactivate your own account");
  users_.setActive(userId, false);
}

void AdminService::resetPassword(const User& actor, int userId, const std::string& newPassword) {
  require(actor, Action::ManageUsers);
  requirePassword(newPassword);
  users_.setPasswordHash(userId, hasher_.hash(newPassword));
}

int AdminService::createDepartment(const User& actor, const std::string& name) {
  require(actor, Action::ManageCourses);
  requireText(name, "department name", 100);
  return courses_.addDepartment(name);
}

int AdminService::createCourse(const User& actor, const std::string& code, const std::string& name,
                               int departmentId) {
  require(actor, Action::ManageCourses);
  requireText(code, "course code", 20);
  requireText(name, "course name", 100);
  if (!courses_.findDepartment(departmentId)) throw NotFound("department not found");
  Course c;
  c.code = code;
  c.name = name;
  c.departmentId = departmentId;
  return courses_.addCourse(c);
}

void AdminService::renameCourse(const User& actor, int courseId, const std::string& name) {
  require(actor, Action::ManageCourses);
  requireText(name, "course name", 100);
  courses_.renameCourse(courseId, name);
}

void AdminService::assignLecturer(const User& actor, int courseId, int lecturerId) {
  require(actor, Action::AssignAndEnrol);
  if (!courses_.findCourse(courseId)) throw NotFound("course not found");
  auto lecturer = users_.findById(lecturerId);
  if (!lecturer) throw NotFound("user not found");
  if (lecturer->role() != Role::Lecturer) throw RuleViolation("only a lecturer can be assigned to a course");
  if (!lecturer->active()) throw RuleViolation("that lecturer's account is deactivated");
  courses_.setLecturer(courseId, lecturerId);
}

void AdminService::enrolStudent(const User& actor, int studentId, int courseId) {
  require(actor, Action::AssignAndEnrol);
  if (!courses_.findCourse(courseId)) throw NotFound("course not found");
  auto student = users_.findById(studentId);
  if (!student) throw NotFound("user not found");
  if (student->role() != Role::Student) throw RuleViolation("only a student can be enrolled");
  if (!student->active()) throw RuleViolation("that student's account is deactivated");
  courses_.enrol(studentId, courseId);
}

}  // namespace lms

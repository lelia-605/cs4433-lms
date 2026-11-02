#include "lms/services/AssignmentService.hpp"

#include <algorithm>

#include "lms/services/InputValidator.hpp"

namespace lms {

Course AssignmentService::ownedCourse(const User& lecturer, int courseId) const {
  auto course = courses_.findCourse(courseId);
  if (!course) throw NotFound("course not found");
  if (course->lecturerId != lecturer.id()) throw AccessDenied("you do not teach this course");
  return *course;
}

int AssignmentService::createAssignment(const User& lecturer, int courseId, const std::string& title,
                                        TimePoint deadline, int maxMark) {
  if (!lecturer.can(Action::CreateAssignment)) throw AccessDenied("you are not allowed to do this");
  ownedCourse(lecturer, courseId);
  if (InputValidator::isBlank(title)) throw RuleViolation("title must not be empty");
  if (title.size() > 200) throw RuleViolation("title is too long (maximum 200)");
  if (maxMark < 1 || maxMark > kMaxMarkLimit)
    throw RuleViolation("maximum mark must be between 1 and " + std::to_string(kMaxMarkLimit));
  if (deadline <= clock_.now()) throw RuleViolation("the deadline must be in the future");
  Assignment a;
  a.courseId = courseId;
  a.title = title;
  a.deadline = deadline;
  a.maxMark = maxMark;
  return assignments_.addAssignment(a);
}

TimePoint AssignmentService::classDeadline(int assignmentId) const {
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  auto cls = assignments_.findClassExtension(assignmentId);
  return cls ? std::max(cls->newDeadline, assignment->deadline) : assignment->deadline;
}

TimePoint AssignmentService::effectiveDeadline(int assignmentId, int studentId) const {
  const TimePoint forClass = classDeadline(assignmentId);
  auto own = assignments_.findExtension(assignmentId, studentId);
  return own ? std::max(own->newDeadline, forClass) : forClass;
}

ExtensionKind AssignmentService::extensionKind(int assignmentId, int studentId) const {
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  const TimePoint deadline = effectiveDeadline(assignmentId, studentId);
  if (deadline == assignment->deadline) return ExtensionKind::None;
  auto own = assignments_.findExtension(assignmentId, studentId);
  return own && own->newDeadline == deadline ? ExtensionKind::Individual : ExtensionKind::Class;
}

void AssignmentService::grantClassExtension(const User& lecturer, int assignmentId,
                                            TimePoint newDeadline, const std::string& reason) {
  if (!lecturer.can(Action::GrantExtension)) throw AccessDenied("you are not allowed to do this");
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  ownedCourse(lecturer, assignment->courseId);
  if (InputValidator::isBlank(reason)) throw RuleViolation("a reason is required");
  if (newDeadline <= classDeadline(assignmentId))
    throw RuleViolation("a class extension must be later than the current class deadline");
  if (newDeadline <= clock_.now()) throw RuleViolation("the extended deadline must be in the future");
  ClassExtension e;
  e.assignmentId = assignmentId;
  e.newDeadline = newDeadline;
  e.grantedBy = lecturer.id();
  e.grantedAt = clock_.now();
  e.reason = reason;
  assignments_.saveClassExtension(e);
}

void AssignmentService::grantExtension(const User& lecturer, int assignmentId, int studentId,
                                       TimePoint newDeadline, const std::string& reason) {
  if (!lecturer.can(Action::GrantExtension)) throw AccessDenied("you are not allowed to do this");
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  ownedCourse(lecturer, assignment->courseId);
  if (!courses_.isEnrolled(studentId, assignment->courseId))
    throw RuleViolation("that student is not enrolled in this course");
  if (InputValidator::isBlank(reason)) throw RuleViolation("a reason is required");
  if (newDeadline <= effectiveDeadline(assignmentId, studentId))
    throw RuleViolation("an extension must be later than the student's current deadline");
  if (newDeadline <= clock_.now()) throw RuleViolation("the extended deadline must be in the future");
  Extension e;
  e.assignmentId = assignmentId;
  e.studentId = studentId;
  e.newDeadline = newDeadline;
  e.grantedBy = lecturer.id();
  e.grantedAt = clock_.now();
  e.reason = reason;
  assignments_.saveExtension(e);
}

std::vector<Course> AssignmentService::coursesForStudent(const User& student) const {
  if (!student.can(Action::ViewCourses)) throw AccessDenied("you are not allowed to do this");
  return courses_.coursesForStudent(student.id());
}

std::vector<AssignmentView> AssignmentService::assignmentsForStudent(const User& student, int courseId) const {
  if (!student.can(Action::ViewCourses)) throw AccessDenied("you are not allowed to do this");
  if (!courses_.isEnrolled(student.id(), courseId)) throw AccessDenied("you are not enrolled in this course");
  std::vector<AssignmentView> out;
  for (const Assignment& a : assignments_.findByCourse(courseId)) {
    const ExtensionKind kind = extensionKind(a.id, student.id());
    out.push_back({a, effectiveDeadline(a.id, student.id()), kind != ExtensionKind::None, kind});
  }
  return out;
}

std::vector<Course> AssignmentService::coursesForLecturer(const User& lecturer) const {
  if (!lecturer.can(Action::CreateAssignment)) throw AccessDenied("you are not allowed to do this");
  return courses_.coursesForLecturer(lecturer.id());
}

std::vector<Assignment> AssignmentService::assignmentsForLecturer(const User& lecturer, int courseId) const {
  if (!lecturer.can(Action::CreateAssignment)) throw AccessDenied("you are not allowed to do this");
  ownedCourse(lecturer, courseId);
  return assignments_.findByCourse(courseId);
}

}  // namespace lms

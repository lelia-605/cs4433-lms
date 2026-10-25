#pragma once
#include <string>
#include <vector>

#include "lms/data/IAssignmentRepository.hpp"
#include "lms/data/ICourseRepository.hpp"
#include "lms/domain/User.hpp"
#include "lms/services/IClock.hpp"

namespace lms {

// What a student sees for one assignment: the deadline that applies to *them* (FR8).
struct AssignmentView {
  Assignment assignment;
  TimePoint effectiveDeadline;
  bool hasExtension = false;
};

// Assignments and individual extensions (FR6, FR7) and the student's course and
// assignment lists (FR8). Lecturers can only work on courses they teach.
class AssignmentService {
 public:
  AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses, const IClock& clock)
      : assignments_(assignments), courses_(courses), clock_(clock) {}

  int createAssignment(const User& lecturer, int courseId, const std::string& title,
                       TimePoint deadline, int maxMark);
  void grantExtension(const User& lecturer, int assignmentId, int studentId,
                      TimePoint newDeadline, const std::string& reason);

  // The extension if the student has one, otherwise the assignment's own deadline (BR2).
  TimePoint effectiveDeadline(int assignmentId, int studentId) const;

  std::vector<Course> coursesForStudent(const User& student) const;
  std::vector<AssignmentView> assignmentsForStudent(const User& student, int courseId) const;

  static constexpr int kMaxMarkLimit = 1000;

 private:
  Course ownedCourse(const User& lecturer, int courseId) const;

  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  const IClock& clock_;
};

}  // namespace lms

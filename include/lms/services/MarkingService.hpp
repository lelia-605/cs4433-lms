#pragma once
#include <string>
#include <vector>

#include "lms/data/IAssignmentRepository.hpp"
#include "lms/data/ICourseRepository.hpp"
#include "lms/data/ISubmissionRepository.hpp"
#include "lms/domain/User.hpp"
#include "lms/services/IClock.hpp"
#include "lms/services/IPenaltyStrategy.hpp"

namespace lms {

// A marked submission with the late penalty worked out (BR1).
struct MarkDetails {
  int rawMark = 0;
  int maxMark = 0;
  int daysLate = 0;
  int penalty = 0;    // marks taken off for lateness
  int finalMark = 0;  // raw mark minus penalty, never below zero; zero after 3 days
  std::string feedback;
  SubmissionState state = SubmissionState::Marked;
};

// Works out the penalty and final mark for a marked submission (used for every role's view).
MarkDetails buildMarkDetails(const Submission& s, const Assignment& a, const IPenaltyStrategy& penalty);

// Lecturer marking and feedback (FR13), mark changes with a reason (FR14) and the
// audit trail of every mark change (FR15). Only the course's own lecturer can mark.
class MarkingService {
 public:
  MarkingService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                 ICourseRepository& courses, const IPenaltyStrategy& penalty, const IClock& clock)
      : submissions_(submissions), assignments_(assignments), courses_(courses),
        penalty_(penalty), clock_(clock) {}

  void markSubmission(const User& lecturer, int assignmentId, int studentId, int rawMark,
                      const std::string& feedback);
  void changeMark(const User& lecturer, int assignmentId, int studentId, int newRawMark,
                  const std::string& feedback, const std::string& reason);

  MarkDetails markDetails(const User& lecturer, int assignmentId, int studentId) const;
  // Visible to the course's lecturer and the Head of the course's department.
  std::vector<AuditEntry> auditTrail(const User& viewer, int assignmentId, int studentId) const;

  static constexpr std::size_t kMaxFeedbackLength = 2000;

 private:
  Assignment lecturersAssignment(const User& lecturer, int assignmentId) const;
  Submission submissionOf(int assignmentId, int studentId) const;
  void checkMarkAndFeedback(const Assignment& a, int rawMark, const std::string& feedback) const;

  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  const IPenaltyStrategy& penalty_;
  const IClock& clock_;
};

}  // namespace lms

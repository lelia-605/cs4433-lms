#pragma once
#include <string>
#include <vector>

#include "lms/data/IAssignmentRepository.hpp"
#include "lms/data/IAuditRepository.hpp"
#include "lms/data/ICourseRepository.hpp"
#include "lms/data/ISubmissionRepository.hpp"
#include "lms/data/ITransactionRunner.hpp"
#include "lms/domain/User.hpp"
#include "lms/services/IClock.hpp"
#include "lms/services/IPenaltyStrategy.hpp"
#include "lms/services/ISubmissionObserver.hpp"

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

// Lecturer marking and feedback (FR13) and changing a mark with a reason (BR6).
// Every change is reported to the attached observers inside one transaction, so the
// audit entry (FR19) and the new state (FR14) are saved with the mark or not at all.
// The audit trail can be viewed by the course's lecturer, the HoD of the department
// and the administrator (FR20). Only the course's own lecturer can mark.
class MarkingService : public SubmissionSubject {
 public:
  MarkingService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                 ICourseRepository& courses, IAuditRepository& audit, ITransactionRunner& transactions,
                 const IPenaltyStrategy& penalty, const IClock& clock)
      : submissions_(submissions), assignments_(assignments), courses_(courses), audit_(audit),
        transactions_(transactions), penalty_(penalty), clock_(clock) {}

  void markSubmission(const User& lecturer, int assignmentId, int studentId, int rawMark,
                      const std::string& feedback);
  void changeMark(const User& lecturer, int assignmentId, int studentId, int newRawMark,
                  const std::string& feedback, const std::string& reason);

  MarkDetails markDetails(const User& lecturer, int assignmentId, int studentId) const;
  std::vector<AuditEntry> auditTrail(const User& viewer, int assignmentId, int studentId) const;

  static constexpr std::size_t kMaxFeedbackLength = 2000;

 private:
  Assignment lecturersAssignment(const User& lecturer, int assignmentId) const;
  Submission submissionOf(int assignmentId, int studentId) const;
  void checkMarkAndFeedback(const Assignment& a, int rawMark, const std::string& feedback) const;
  AuditEntry changeEntry(const User& by, const Submission& s, int newValue, const std::string& reason) const;

  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IAuditRepository& audit_;
  ITransactionRunner& transactions_;
  const IPenaltyStrategy& penalty_;
  const IClock& clock_;
};

}  // namespace lms

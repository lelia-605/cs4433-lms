#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/data/ISubmissionRepository.hpp"
#include "lms/services/AssignmentService.hpp"

namespace lms {

// One row of the lecturer's submission list (FR21). Students who have not
// submitted yet are included with submitted = false.
struct SubmissionStatus {
  int studentId = 0;
  bool submitted = false;
  std::optional<Submission> submission;
  int versionCount = 0;
};

// Submitting (FR9), resubmitting before the deadline (FR10, BR3) and the lecturer's
// status list (FR21). Late work is always accepted (FR9, FR12); the days late are worked
// out from the student's effective deadline, so an extension is taken into account (BR2).
class SubmissionService {
 public:
  SubmissionService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                    ICourseRepository& courses, const AssignmentService& assignmentService,
                    const IClock& clock)
      : submissions_(submissions), assignments_(assignments), courses_(courses),
        assignmentService_(assignmentService), clock_(clock) {}

  // Returns the version number that was saved (1 for a first submission).
  int submit(const User& student, int assignmentId, const std::string& content);

  std::optional<Submission> mySubmission(const User& student, int assignmentId) const;
  std::vector<SubmissionVersion> myVersions(const User& student, int assignmentId) const;

  std::vector<SubmissionStatus> statusForAssignment(const User& lecturer, int assignmentId) const;

  static constexpr std::size_t kMaxContentLength = 10000;

 private:
  Assignment studentAssignment(const User& student, int assignmentId) const;

  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  const AssignmentService& assignmentService_;
  const IClock& clock_;
};

}  // namespace lms

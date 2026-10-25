#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/services/MarkingService.hpp"

namespace lms {

// One marked submission waiting for the HoD, with its final mark worked out.
struct ModerationItem {
  Submission submission;
  Assignment assignment;
  MarkDetails details;
};

// What a student sees for one assignment. Mark and feedback are only filled in
// once the HoD has released them (FR19, BR4).
struct StudentResult {
  bool submitted = false;
  bool released = false;
  SubmissionState state = SubmissionState::Submitted;
  std::optional<MarkDetails> details;  // empty until released
};

// HoD moderation (FR16 approve, FR17 adjust with a reason), release of approved
// marks (FR18) and the student's view of released results (FR19).
// The HoD can only moderate courses in their own department.
class ModerationService {
 public:
  ModerationService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                    ICourseRepository& courses, const IPenaltyStrategy& penalty, const IClock& clock)
      : submissions_(submissions), assignments_(assignments), courses_(courses),
        penalty_(penalty), clock_(clock) {}

  std::vector<ModerationItem> awaitingModeration(const User& hod) const;
  void approve(const User& hod, int submissionId);
  void adjust(const User& hod, int submissionId, int newRawMark, const std::string& reason);
  // Releases every approved mark for the assignment; returns how many were released.
  int release(const User& hod, int assignmentId);

  StudentResult resultFor(const User& student, int assignmentId) const;

 private:
  const HeadOfDepartment& asHod(const User& user, Action action) const;
  Assignment departmentAssignment(const HeadOfDepartment& hod, int assignmentId) const;
  Submission markedSubmission(const HeadOfDepartment& hod, int submissionId, Assignment& out) const;

  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  const IPenaltyStrategy& penalty_;
  const IClock& clock_;
};

}  // namespace lms

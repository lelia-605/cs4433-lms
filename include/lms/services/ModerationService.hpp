#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/services/MarkingService.hpp"

namespace lms {

// One marked submission waiting for the HoD, with what FR15 asks to show:
// the lecturer, the penalty applied and any extension granted.
struct ModerationItem {
  Submission submission;
  Assignment assignment;
  MarkDetails details;
  std::optional<int> lecturerId;
  std::optional<Extension> extension;
};

// What a student sees for one assignment. Mark and feedback are only filled in
// once the HoD has released them (FR18, BR4).
struct StudentResult {
  bool submitted = false;
  bool released = false;
  SubmissionState state = SubmissionState::Submitted;
  std::optional<MarkDetails> details;  // empty until released
};

// HoD moderation (UC09-UC11): list marked work (FR15), approve or adjust with a
// reason (FR16), bulk approval (UC09 A2), release (FR17, BR5) and the student's view
// of released results (FR18). Observers record every change in the same transaction.
// The HoD can only moderate courses in their own department (NFR6).
class ModerationService : public SubmissionSubject {
 public:
  ModerationService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                    ICourseRepository& courses, ITransactionRunner& transactions,
                    const IPenaltyStrategy& penalty, const IClock& clock)
      : submissions_(submissions), assignments_(assignments), courses_(courses),
        transactions_(transactions), penalty_(penalty), clock_(clock) {}

  std::vector<ModerationItem> listForModeration(const User& hod) const;
  void approve(const User& hod, int submissionId);
  void adjustMark(const User& hod, int submissionId, int newRawMark, const std::string& reason);
  // Approves every marked submission of the assignment; returns how many (UC09 A2).
  int approveAll(const User& hod, int assignmentId);
  // Releases the moderated marks of the assignment and returns how many. If some
  // submissions are not moderated yet, it refuses and names them (UC09 E3), unless
  // partial is true, which releases only the moderated ones (UC09 A3).
  int release(const User& hod, int assignmentId, bool partial = false);

  StudentResult resultFor(const User& student, int assignmentId) const;
  // Every assignment on a course in the HoD's department (to pick one to release).
  std::vector<Assignment> departmentAssignments(const User& hod) const;

 private:
  const HeadOfDepartment& asHod(const User& user, Action action) const;
  Assignment departmentAssignment(const HeadOfDepartment& hod, int assignmentId) const;
  Submission markedSubmission(const HeadOfDepartment& hod, int submissionId, Assignment& out) const;
  void moderate(const HeadOfDepartment& hod, Submission s, int newRawMark, const std::string& reason);

  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  ITransactionRunner& transactions_;
  const IPenaltyStrategy& penalty_;
  const IClock& clock_;
};

}  // namespace lms

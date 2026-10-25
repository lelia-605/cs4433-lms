#pragma once
#include <optional>
#include <string>

#include "lms/Errors.hpp"
#include "lms/domain/Time.hpp"

namespace lms {

// Life cycle of a submission (state machine in the Phase 3 design).
enum class SubmissionState { Submitted, Late, Marked, Moderated, Released };

// State <-> text stored in submission.state (matches the CHECK constraint).
inline std::string stateToString(SubmissionState s) {
  switch (s) {
    case SubmissionState::Submitted: return "SUBMITTED";
    case SubmissionState::Late:      return "LATE";
    case SubmissionState::Marked:    return "MARKED";
    case SubmissionState::Moderated: return "MODERATED";
    case SubmissionState::Released:  return "RELEASED";
  }
  throw RuleViolation("unknown submission state");
}

inline SubmissionState stateFromString(const std::string& s) {
  if (s == "SUBMITTED") return SubmissionState::Submitted;
  if (s == "LATE")      return SubmissionState::Late;
  if (s == "MARKED")    return SubmissionState::Marked;
  if (s == "MODERATED") return SubmissionState::Moderated;
  if (s == "RELEASED")  return SubmissionState::Released;
  throw RuleViolation("unknown submission state: " + s);
}

// One student's submission for one assignment.
struct Submission {
  int id = 0;
  int assignmentId = 0;
  int studentId = 0;
  SubmissionState state = SubmissionState::Submitted;
  TimePoint submittedAt;  // time of the latest version
  int daysLate = 0;
  std::optional<int> rawMark;  // the lecturer's mark before any late penalty (FR13)
  std::string feedback;
  std::optional<int> markedBy;
  std::optional<int> moderatedBy;    // the HoD who approved or adjusted the mark (FR16, FR17)
  std::optional<TimePoint> releasedAt;  // set when the HoD releases the mark (FR18)
};

// Every upload is kept as a numbered version (FR11).
struct SubmissionVersion {
  int versionNo = 0;
  std::string content;
  TimePoint submittedAt;
};

}  // namespace lms

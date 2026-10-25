#include "lms/domain/Submission.hpp"

#include "lms/services/IPenaltyStrategy.hpp"

namespace lms {

bool Submission::canMove(SubmissionState from, SubmissionState to) {
  using S = SubmissionState;
  switch (from) {
    case S::Submitted:
    case S::Late:
      // resubmission (and an extension can turn Late back into Submitted), or marking
      return to == S::Submitted || to == S::Late || to == S::Marked;
    case S::Marked:    return to == S::Moderated;
    case S::Moderated: return to == S::Released;
    case S::Released:  return false;  // final state
  }
  return false;
}

void Submission::transitionTo(SubmissionState next) {
  if (!canMove(state, next))
    throw RuleViolation("a submission cannot go from " + stateToString(state) + " to " + stateToString(next));
  state = next;
}

int Submission::finalMark(int maxMark, const IPenaltyStrategy& strategy) const {
  if (!rawMark) return 0;
  return lms::finalMark(*rawMark, maxMark, daysLate, strategy);
}

}  // namespace lms

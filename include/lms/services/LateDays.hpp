#pragma once
#include <chrono>

namespace lms {

using TimePoint = std::chrono::system_clock::time_point;

// Days late = number of started 24-hour periods after the effective deadline (rule C4).
//   at or before the deadline -> 0     1 second late -> 1
//   exactly 72 h late         -> 3     72 h + 1 second -> 4 (more than 3 days)
inline int daysLate(TimePoint effectiveDeadline, TimePoint submittedAt) {
  using namespace std::chrono;
  if (submittedAt <= effectiveDeadline) return 0;
  const auto late = duration_cast<seconds>(submittedAt - effectiveDeadline).count();
  const long long day = 24LL * 60 * 60;
  return static_cast<int>((late + day - 1) / day);
}

}  // namespace lms

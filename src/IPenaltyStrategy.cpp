#include "lms/services/IPenaltyStrategy.hpp"

#include <algorithm>

namespace lms {

int StandardPenaltyStrategy::penaltyFor(int maxMark, int daysLate) const {
  if (daysLate <= 0) return 0;
  const int days = std::min(daysLate, maxLateDays_);
  return (maxMark * percentPerDay_ * days + 50) / 100;  // round half up
}

int finalMark(int rawMark, int maxMark, int daysLate, const IPenaltyStrategy& strategy) {
  if (strategy.isZeroMark(daysLate)) return 0;
  return std::max(0, rawMark - strategy.penaltyFor(maxMark, daysLate));
}

}  // namespace lms

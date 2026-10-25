#pragma once

namespace lms {

// Strategy pattern: the late-penalty rule can be replaced or tested on its own.
class IPenaltyStrategy {
 public:
  virtual ~IPenaltyStrategy() = default;
  // Marks to deduct from the raw mark for a submission that is daysLate days late.
  virtual int penaltyFor(int maxMark, int daysLate) const = 0;
  // True when the mark must be zero whatever the raw mark is.
  virtual bool isZeroMark(int daysLate) const = 0;
};

// Rule agreed in Phase 2 (BR1): 10% of the maximum mark per day, days 1 to 3,
// zero beyond 3 days. The deduction is rounded to the nearest whole mark.
class StandardPenaltyStrategy : public IPenaltyStrategy {
 public:
  explicit StandardPenaltyStrategy(int percentPerDay = 10, int maxLateDays = 3)
      : percentPerDay_(percentPerDay), maxLateDays_(maxLateDays) {}
  int penaltyFor(int maxMark, int daysLate) const override;
  bool isZeroMark(int daysLate) const override { return daysLate > maxLateDays_; }

 private:
  int percentPerDay_;
  int maxLateDays_;
};

// Final mark = raw mark minus the penalty, never below zero (or zero when too late).
int finalMark(int rawMark, int maxMark, int daysLate, const IPenaltyStrategy& strategy);

}  // namespace lms

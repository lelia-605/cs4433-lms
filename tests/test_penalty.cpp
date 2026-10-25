#include <gtest/gtest.h>

#include "lms/services/IPenaltyStrategy.hpp"
#include "lms/services/LateDays.hpp"

using namespace lms;
using namespace std::chrono;

// Boundary value analysis for the late rule (SRS table: max 50, raw 40).
TEST(DaysLate, BoundaryValues) {
  const TimePoint d = system_clock::time_point{} + hours(1000);
  EXPECT_EQ(daysLate(d, d - seconds(1)), 0);           // before the deadline
  EXPECT_EQ(daysLate(d, d), 0);                        // exactly on the deadline
  EXPECT_EQ(daysLate(d, d + seconds(1)), 1);           // 1 second late
  EXPECT_EQ(daysLate(d, d + hours(24)), 1);            // exactly 24 h
  EXPECT_EQ(daysLate(d, d + hours(24) + seconds(1)), 2);
  EXPECT_EQ(daysLate(d, d + hours(72)), 3);            // exactly 72 h
  EXPECT_EQ(daysLate(d, d + hours(72) + seconds(1)), 4);  // beyond 3 days
}

TEST(Penalty, SrsExampleTable) {
  StandardPenaltyStrategy s;
  EXPECT_EQ(finalMark(40, 50, 0, s), 40);
  EXPECT_EQ(finalMark(40, 50, 1, s), 35);
  EXPECT_EQ(finalMark(40, 50, 2, s), 30);
  EXPECT_EQ(finalMark(40, 50, 3, s), 25);
  EXPECT_EQ(finalMark(40, 50, 4, s), 0);   // more than 3 days: zero
}

TEST(Penalty, NeverBelowZeroAndRounds) {
  StandardPenaltyStrategy s;
  EXPECT_EQ(finalMark(5, 100, 3, s), 0);       // 30 deducted from 5 -> 0, not negative
  EXPECT_EQ(s.penaltyFor(33, 1), 3);           // 3.3 rounds to 3
  EXPECT_EQ(s.penaltyFor(35, 1), 4);           // 3.5 rounds up to 4
  EXPECT_TRUE(s.isZeroMark(4));
  EXPECT_FALSE(s.isZeroMark(3));
}

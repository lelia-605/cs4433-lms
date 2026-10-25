#pragma once
#include <chrono>

#include "lms/domain/Time.hpp"

namespace lms {

// Services read the time through this interface so tests can control it.
class IClock {
 public:
  virtual ~IClock() = default;
  virtual TimePoint now() const = 0;
};

class SystemClock : public IClock {
 public:
  TimePoint now() const override { return std::chrono::system_clock::now(); }
};

// For tests: time stands still until set() or advance() is called.
class FixedClock : public IClock {
 public:
  explicit FixedClock(TimePoint t) : now_(t) {}
  TimePoint now() const override { return now_; }
  void set(TimePoint t) { now_ = t; }
  void advance(std::chrono::seconds s) { now_ += s; }

 private:
  TimePoint now_;
};

}  // namespace lms

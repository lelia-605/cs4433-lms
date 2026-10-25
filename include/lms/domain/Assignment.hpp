#pragma once
#include <string>

#include "lms/domain/Time.hpp"

namespace lms {

struct Assignment {
  int id = 0;
  int courseId = 0;
  std::string title;
  TimePoint deadline;
  int maxMark = 0;
};

// An individual deadline extension for one student on one assignment (FR7).
struct Extension {
  int id = 0;
  int assignmentId = 0;
  int studentId = 0;
  TimePoint newDeadline;
  int grantedBy = 0;
  TimePoint grantedAt;
  std::string reason;
};

}  // namespace lms

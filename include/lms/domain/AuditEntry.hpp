#pragma once
#include <optional>
#include <string>

#include "lms/domain/Time.hpp"

namespace lms {

// One row of the append-only audit log: who changed a mark, when, from what to what, and why.
struct AuditEntry {
  int id = 0;
  int submissionId = 0;
  int changedBy = 0;
  TimePoint changedAt;
  std::optional<int> oldValue;  // empty for the first marking
  std::optional<int> newValue;
  std::string reason;
};

}  // namespace lms

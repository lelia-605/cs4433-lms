#pragma once
#include <optional>
#include <string>

#include "lms/domain/Time.hpp"

namespace lms {

// Checks that input is well formed before it is used (NFR7). The console uses the
// parse functions on what the user types; the services use the require functions,
// which throw RuleViolation, so a rule is never skipped even if the console changes.
class InputValidator {
 public:
  static bool isBlank(const std::string& s);
  static void requireText(const std::string& value, const std::string& field, std::size_t maxLength);
  static void requireRange(int value, int min, int max, const std::string& field);

  static std::optional<int> parseInt(const std::string& text);
  // "YYYY-MM-DD HH:MM" in UTC, e.g. "2026-11-01 10:00".
  static std::optional<TimePoint> parseDateTime(const std::string& text);
};

}  // namespace lms

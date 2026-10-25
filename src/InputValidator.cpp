#include "lms/services/InputValidator.hpp"

#include <algorithm>
#include <cctype>

#include "lms/Errors.hpp"

namespace lms {

bool InputValidator::isBlank(const std::string& s) {
  return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}

void InputValidator::requireText(const std::string& value, const std::string& field, std::size_t maxLength) {
  if (isBlank(value)) throw RuleViolation(field + " must not be empty");
  if (value.size() > maxLength)
    throw RuleViolation(field + " is too long (maximum " + std::to_string(maxLength) + ")");
}

void InputValidator::requireRange(int value, int min, int max, const std::string& field) {
  if (value < min || value > max)
    throw RuleViolation(field + " must be between " + std::to_string(min) + " and " + std::to_string(max));
}

std::optional<int> InputValidator::parseInt(const std::string& text) {
  try {
    std::size_t used = 0;
    const int value = std::stoi(text, &used);
    if (used == text.size()) return value;
  } catch (const std::exception&) {
  }
  return std::nullopt;
}

std::optional<TimePoint> InputValidator::parseDateTime(const std::string& text) {
  if (text.size() != 16 || text[10] != ' ') return std::nullopt;
  try {
    return fromIso(text.substr(0, 10) + "T" + text.substr(11) + ":00Z");
  } catch (const RuleViolation&) {
    return std::nullopt;
  }
}

}  // namespace lms

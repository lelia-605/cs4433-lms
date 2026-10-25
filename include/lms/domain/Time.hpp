#pragma once
#include <chrono>
#include <string>

namespace lms {

using TimePoint = std::chrono::system_clock::time_point;

// All times are stored as UTC text "YYYY-MM-DDTHH:MM:SSZ" (ISO-8601), see the schema.
std::string toIso(TimePoint tp);
TimePoint fromIso(const std::string& text);  // throws RuleViolation if the text is not a valid date and time

}  // namespace lms

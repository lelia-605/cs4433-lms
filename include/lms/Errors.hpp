#pragma once
#include <stdexcept>

namespace lms {

// All errors the business and data layers can throw. The console catches these,
// shows a message and carries on, so the program never crashes (NFR7).
struct RuleViolation : std::runtime_error { using std::runtime_error::runtime_error; };
struct AccessDenied  : std::runtime_error { using std::runtime_error::runtime_error; };
struct NotFound      : std::runtime_error { using std::runtime_error::runtime_error; };
struct DatabaseError : std::runtime_error { using std::runtime_error::runtime_error; };

}  // namespace lms

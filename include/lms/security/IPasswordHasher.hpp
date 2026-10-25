#pragma once
#include <string>

namespace lms {

// Abstraction over password hashing (dependency inversion).
class IPasswordHasher {
 public:
  virtual ~IPasswordHasher() = default;
  virtual std::string hash(const std::string& plain) const = 0;
  virtual bool verify(const std::string& plain, const std::string& hash) const = 0;
};

}  // namespace lms

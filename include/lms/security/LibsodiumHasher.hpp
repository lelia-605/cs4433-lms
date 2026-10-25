#pragma once
#include "lms/security/IPasswordHasher.hpp"

namespace lms {

// Argon2id through libsodium's crypto_pwhash_str. The random salt is generated
// by the library and stored inside the returned string, so nothing else is needed.
class LibsodiumHasher : public IPasswordHasher {
 public:
  LibsodiumHasher();
  std::string hash(const std::string& plain) const override;
  bool verify(const std::string& plain, const std::string& hash) const override;
};

}  // namespace lms

#include "lms/security/LibsodiumHasher.hpp"

#include <sodium.h>

#include <stdexcept>

namespace lms {

LibsodiumHasher::LibsodiumHasher() {
  if (sodium_init() < 0) throw std::runtime_error("libsodium failed to initialise");
}

std::string LibsodiumHasher::hash(const std::string& plain) const {
  char out[crypto_pwhash_STRBYTES];
  if (crypto_pwhash_str(out, plain.c_str(), plain.size(),
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    throw std::runtime_error("password hashing failed (out of memory)");
  return std::string(out);
}

bool LibsodiumHasher::verify(const std::string& plain, const std::string& hash) const {
  return crypto_pwhash_str_verify(hash.c_str(), plain.c_str(), plain.size()) == 0;
}

}  // namespace lms

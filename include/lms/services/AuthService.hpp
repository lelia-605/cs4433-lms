#pragma once
#include <memory>

#include "lms/data/IUserRepository.hpp"
#include "lms/security/IPasswordHasher.hpp"

namespace lms {

class AuthService {
 public:
  AuthService(IUserRepository& users, const IPasswordHasher& hasher) : users_(users), hasher_(hasher) {}

  // Returns the user, or nullptr. The caller cannot tell a wrong username from a
  // wrong password, so the login screen can show one generic message (FR1).
  std::unique_ptr<User> login(const std::string& username, const std::string& password) const;

 private:
  IUserRepository& users_;
  const IPasswordHasher& hasher_;
};

}  // namespace lms

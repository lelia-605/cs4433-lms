#include "lms/services/AuthService.hpp"

namespace lms {

std::unique_ptr<User> AuthService::login(const std::string& username, const std::string& password) const {
  auto user = users_.findByUsername(username);
  if (!user || !user->verifyPassword(password, hasher_)) return nullptr;
  return user;
}

}  // namespace lms

#pragma once
#include <memory>
#include <optional>
#include <string>

#include "lms/domain/User.hpp"

namespace lms {

struct UserData {
  int id = 0;
  std::string username, passwordHash, fullName;
  bool active = true;
  std::optional<int> departmentId;  // required for lecturers and HoDs
};

// Factory pattern: the only place in the program that switches on the role.
class UserFactory {
 public:
  static std::unique_ptr<User> create(Role role, const UserData& d);
};

}  // namespace lms

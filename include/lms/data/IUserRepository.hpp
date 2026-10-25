#pragma once
#include <memory>
#include <optional>
#include <string>

#include "lms/domain/UserFactory.hpp"

namespace lms {

struct NewUser {
  std::string username, passwordHash, fullName;
  Role role = Role::Student;
  std::optional<int> departmentId;
};

// Repository pattern: services use this interface and never see SQL.
class IUserRepository {
 public:
  virtual ~IUserRepository() = default;
  virtual int add(const NewUser& u) = 0;
  virtual std::unique_ptr<User> findByUsername(const std::string& username) const = 0;
};

}  // namespace lms

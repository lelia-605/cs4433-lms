#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
  virtual std::unique_ptr<User> findById(int id) const = 0;
  virtual void setActive(int id, bool active) = 0;
  virtual void updateProfile(int id, const std::string& fullName, std::optional<int> departmentId) = 0;
  virtual void setPasswordHash(int id, const std::string& hash) = 0;
  virtual std::vector<std::unique_ptr<User>> allUsers() const = 0;  // ordered by id
};

}  // namespace lms

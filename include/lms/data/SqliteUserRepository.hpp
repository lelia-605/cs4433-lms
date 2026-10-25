#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/IUserRepository.hpp"

namespace lms {

class SqliteUserRepository : public IUserRepository {
 public:
  explicit SqliteUserRepository(DatabaseConnection& db) : db_(db) {}
  int add(const NewUser& u) override;
  std::unique_ptr<User> findByUsername(const std::string& username) const override;
  std::unique_ptr<User> findById(int id) const override;
  void setActive(int id, bool active) override;
  void updateProfile(int id, const std::string& fullName, std::optional<int> departmentId) override;
  void setPasswordHash(int id, const std::string& hash) override;
  std::vector<std::unique_ptr<User>> allUsers() const override;

 private:
  DatabaseConnection& db_;
};

}  // namespace lms

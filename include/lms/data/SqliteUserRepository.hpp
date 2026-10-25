#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/IUserRepository.hpp"

namespace lms {

class SqliteUserRepository : public IUserRepository {
 public:
  explicit SqliteUserRepository(DatabaseConnection& db) : db_(db) {}
  int add(const NewUser& u) override;
  std::unique_ptr<User> findByUsername(const std::string& username) const override;

 private:
  DatabaseConnection& db_;
};

}  // namespace lms

#include "lms/data/SqliteUserRepository.hpp"

namespace lms {

int SqliteUserRepository::add(const NewUser& u) {
  auto st = db_.prepare(
      "INSERT INTO app_user (username, password_hash, full_name, role, department_id) "
      "VALUES (?, ?, ?, ?, ?)");
  st.bind(1, u.username);
  st.bind(2, u.passwordHash);
  st.bind(3, u.fullName);
  st.bind(4, roleToString(u.role));
  if (u.departmentId) st.bind(5, *u.departmentId); else st.bindNull(5);
  try {
    st.step();
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string("cannot add user: ") + e.what());  // e.g. duplicate username
  }
  return db_.lastInsertId();
}

std::unique_ptr<User> SqliteUserRepository::findByUsername(const std::string& username) const {
  auto st = db_.prepare(
      "SELECT user_id, username, password_hash, full_name, role, is_active, department_id "
      "FROM app_user WHERE username = ?");
  st.bind(1, username);
  if (!st.step()) return nullptr;
  UserData d;
  d.id = st.columnInt(0);
  d.username = st.columnText(1);
  d.passwordHash = st.columnText(2);
  d.fullName = st.columnText(3);
  d.active = st.columnInt(5) != 0;
  if (!st.columnIsNull(6)) d.departmentId = st.columnInt(6);
  return UserFactory::create(roleFromString(st.columnText(4)), d);
}

}  // namespace lms

#include "lms/data/SqliteUserRepository.hpp"

namespace lms {

namespace {

const char* kSelectUser =
    "SELECT user_id, username, password_hash, full_name, role, is_active, department_id "
    "FROM app_user ";

std::unique_ptr<User> readUser(Statement& st) {
  UserData d;
  d.id = st.columnInt(0);
  d.username = st.columnText(1);
  d.passwordHash = st.columnText(2);
  d.fullName = st.columnText(3);
  d.active = st.columnInt(5) != 0;
  if (!st.columnIsNull(6)) d.departmentId = st.columnInt(6);
  return UserFactory::create(roleFromString(st.columnText(4)), d);
}

// Runs an UPDATE and turns "no row changed" into NotFound and constraint errors into RuleViolation.
void runUpdate(DatabaseConnection& db, Statement& st) {
  try {
    st.step();
  } catch (const DatabaseError& e) {
    throw RuleViolation(std::string("update rejected: ") + e.what());
  }
  if (db.changes() == 0) throw NotFound("user not found");
}

}  // namespace

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
  auto st = db_.prepare(std::string(kSelectUser) + "WHERE username = ?");
  st.bind(1, username);
  if (!st.step()) return nullptr;
  return readUser(st);
}

std::unique_ptr<User> SqliteUserRepository::findById(int id) const {
  auto st = db_.prepare(std::string(kSelectUser) + "WHERE user_id = ?");
  st.bind(1, id);
  if (!st.step()) return nullptr;
  return readUser(st);
}

void SqliteUserRepository::setActive(int id, bool active) {
  auto st = db_.prepare("UPDATE app_user SET is_active = ? WHERE user_id = ?");
  st.bind(1, active ? 1 : 0);
  st.bind(2, id);
  runUpdate(db_, st);
}

void SqliteUserRepository::updateProfile(int id, const std::string& fullName,
                                         std::optional<int> departmentId) {
  auto st = db_.prepare("UPDATE app_user SET full_name = ?, department_id = ? WHERE user_id = ?");
  st.bind(1, fullName);
  if (departmentId) st.bind(2, *departmentId); else st.bindNull(2);
  st.bind(3, id);
  runUpdate(db_, st);
}

void SqliteUserRepository::setPasswordHash(int id, const std::string& hash) {
  auto st = db_.prepare("UPDATE app_user SET password_hash = ? WHERE user_id = ?");
  st.bind(1, hash);
  st.bind(2, id);
  runUpdate(db_, st);
}

std::vector<std::unique_ptr<User>> SqliteUserRepository::allUsers() const {
  auto st = db_.prepare(std::string(kSelectUser) + "ORDER BY user_id");
  std::vector<std::unique_ptr<User>> out;
  while (st.step()) out.push_back(readUser(st));
  return out;
}

}  // namespace lms

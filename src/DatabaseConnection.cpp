#include "lms/data/DatabaseConnection.hpp"

#include <fstream>
#include <sstream>

namespace lms {

static void check(int rc, sqlite3* db, const char* what) {
  if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
    throw DatabaseError(std::string(what) + ": " + sqlite3_errmsg(db));
}

Statement::Statement(sqlite3* db, const std::string& sql) : stmt_(nullptr), db_(db) {
  check(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr), db, "prepare failed");
}
Statement::~Statement() { if (stmt_) sqlite3_finalize(stmt_); }

void Statement::bind(int i, int v) { check(sqlite3_bind_int(stmt_, i, v), db_, "bind failed"); }
void Statement::bind(int i, const std::string& v) {
  check(sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT), db_, "bind failed");
}
void Statement::bindNull(int i) { check(sqlite3_bind_null(stmt_, i), db_, "bind failed"); }

bool Statement::step() {
  const int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throw DatabaseError(std::string("step failed: ") + sqlite3_errmsg(db_));
}
int Statement::columnInt(int c) const { return sqlite3_column_int(stmt_, c); }
std::string Statement::columnText(int c) const {
  const unsigned char* t = sqlite3_column_text(stmt_, c);
  return t ? reinterpret_cast<const char*>(t) : std::string();
}
bool Statement::columnIsNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }

DatabaseConnection::DatabaseConnection(const std::string& path) {
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    const std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
    sqlite3_close(db_);
    throw DatabaseError("cannot open database: " + msg);
  }
  exec("PRAGMA foreign_keys = ON");  // SQLite leaves this off by default
}
DatabaseConnection::~DatabaseConnection() { if (db_) sqlite3_close(db_); }

void DatabaseConnection::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    const std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    throw DatabaseError("exec failed: " + msg);
  }
}

void DatabaseConnection::runScript(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw DatabaseError("cannot read script: " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  exec(ss.str());
}

Statement DatabaseConnection::prepare(const std::string& sql) { return Statement(db_, sql); }
int DatabaseConnection::lastInsertId() const { return static_cast<int>(sqlite3_last_insert_rowid(db_)); }

}  // namespace lms

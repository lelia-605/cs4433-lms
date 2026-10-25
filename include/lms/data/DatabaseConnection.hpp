#pragma once
#include <sqlite3.h>

#include <string>

#include "lms/Errors.hpp"

namespace lms {

// A prepared statement. Values are always bound as parameters, never pasted into the SQL.
class Statement {
 public:
  Statement(sqlite3* db, const std::string& sql);
  ~Statement();
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement(Statement&& o) noexcept : stmt_(o.stmt_), db_(o.db_) { o.stmt_ = nullptr; }

  void bind(int index, int value);
  void bind(int index, const std::string& value);
  void bindNull(int index);
  bool step();  // true while a row is available
  int columnInt(int col) const;
  std::string columnText(int col) const;
  bool columnIsNull(int col) const;

 private:
  sqlite3_stmt* stmt_;
  sqlite3* db_;
};

// RAII wrapper over the SQLite handle (closed automatically, even after an exception).
class DatabaseConnection {
 public:
  explicit DatabaseConnection(const std::string& path);  // ":memory:" for tests
  ~DatabaseConnection();
  DatabaseConnection(const DatabaseConnection&) = delete;
  DatabaseConnection& operator=(const DatabaseConnection&) = delete;

  void exec(const std::string& sql);        // for fixed SQL such as the schema
  void runScript(const std::string& path);  // run a .sql file (schema.sql)
  Statement prepare(const std::string& sql);
  int lastInsertId() const;

 private:
  sqlite3* db_ = nullptr;
};

// RAII transaction: rolls back unless commit() is called (used with the audit log).
class Transaction {
 public:
  explicit Transaction(DatabaseConnection& db) : db_(db) { db_.exec("BEGIN"); }
  ~Transaction() {
    if (!done_) {
      try { db_.exec("ROLLBACK"); } catch (...) {}
    }
  }
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  void commit() { db_.exec("COMMIT"); done_ = true; }

 private:
  DatabaseConnection& db_;
  bool done_ = false;
};

}  // namespace lms

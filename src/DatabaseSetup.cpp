#include "lms/app/DatabaseSetup.hpp"

namespace lms {

namespace {
// v1.1 adds one table (CR1). A database made by v1.0 gets it on the next start;
// nothing else changes, so all existing data is kept.
const char* kClassExtensionTable =
    "CREATE TABLE class_extension ("
    "  assignment_id INTEGER PRIMARY KEY REFERENCES assignment(assignment_id),"
    "  new_deadline TEXT NOT NULL,"
    "  granted_by INTEGER NOT NULL REFERENCES app_user(user_id),"
    "  granted_at TEXT NOT NULL,"
    "  reason TEXT NOT NULL)";
}  // namespace

void prepareDatabase(DatabaseConnection& db, const std::string& schemaPath, IUserRepository& users,
                     const IPasswordHasher& hasher, std::ostream& out) {
  if (!db.hasTable("app_user")) {
    db.runScript(schemaPath);
    out << "New database created.\n";
  } else if (!db.hasTable("class_extension")) {
    db.exec(kClassExtensionTable);
    out << "Database upgraded to version 1.1.\n";
  }
  if (users.allUsers().empty()) {
    users.add({kFirstAdminUsername, hasher.hash(kFirstAdminPassword), "System Administrator",
               Role::Administrator, std::nullopt});
    out << "First administrator created: username '" << kFirstAdminUsername << "', password '"
        << kFirstAdminPassword << "'. Change this password after logging in.\n";
  }
}

}  // namespace lms

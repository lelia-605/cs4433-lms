#include "lms/app/DatabaseSetup.hpp"

namespace lms {

void prepareDatabase(DatabaseConnection& db, const std::string& schemaPath, IUserRepository& users,
                     const IPasswordHasher& hasher, std::ostream& out) {
  if (!db.hasTable("app_user")) {
    db.runScript(schemaPath);
    out << "New database created.\n";
  }
  if (users.allUsers().empty()) {
    users.add({kFirstAdminUsername, hasher.hash(kFirstAdminPassword), "System Administrator",
               Role::Administrator, std::nullopt});
    out << "First administrator created: username '" << kFirstAdminUsername << "', password '"
        << kFirstAdminPassword << "'. Change this password after logging in.\n";
  }
}

}  // namespace lms

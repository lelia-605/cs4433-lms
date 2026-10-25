#pragma once
#include <ostream>
#include <string>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/IUserRepository.hpp"
#include "lms/security/IPasswordHasher.hpp"

namespace lms {

// Username and password of the administrator created on the very first start.
constexpr const char* kFirstAdminUsername = "admin";
constexpr const char* kFirstAdminPassword = "admin12345";

// Creates the tables if the database file is new, and adds the first administrator
// if there are no users yet. Running it again on an existing database changes nothing.
void prepareDatabase(DatabaseConnection& db, const std::string& schemaPath, IUserRepository& users,
                     const IPasswordHasher& hasher, std::ostream& out);

}  // namespace lms

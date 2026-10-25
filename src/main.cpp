// Walking skeleton: creates an in-memory database, adds one user per role,
// and logs each in. Replace with the real console application as you build.
#include <iostream>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AuthService.hpp"

int main() {
  using namespace lms;
  try {
    DatabaseConnection db(":memory:");
    db.runScript(LMS_SCHEMA_PATH);
    db.exec("INSERT INTO department (name) VALUES ('Science')");

    LibsodiumHasher hasher;
    SqliteUserRepository users(db);
    users.add({"admin", hasher.hash("admin-pass"), "Ada Admin", Role::Administrator, std::nullopt});
    users.add({"lecturer", hasher.hash("lect-pass"), "Leo Lecturer", Role::Lecturer, 1});
    users.add({"hod", hasher.hash("hod-pass"), "Hana HoD", Role::HeadOfDepartment, 1});
    users.add({"student", hasher.hash("stud-pass"), "Sam Student", Role::Student, std::nullopt});

    AuthService auth(users, hasher);
    const char* names[] = {"admin", "lecturer", "hod", "student"};
    const char* pass[] = {"admin-pass", "lect-pass", "hod-pass", "stud-pass"};
    for (int i = 0; i < 4; ++i) {
      auto u = auth.login(names[i], pass[i]);
      std::cout << names[i] << ": " << (u ? u->menuTitle() : "LOGIN FAILED") << "\n";
    }
    std::cout << "wrong password: " << (auth.login("admin", "nope") ? "LOGGED IN (bug)" : "rejected") << "\n";
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}

// Moderated LMS console application.
// Usage: lms [database file]   (default: lms.db in the current folder)
// The data is kept in the SQLite file, so it is still there next time (NFR3).
#include <iostream>

#include "lms/app/DatabaseSetup.hpp"
#include "lms/ui/ConsoleApp.hpp"

int main(int argc, char* argv[]) {
  using namespace lms;
  const std::string dbPath = argc > 1 ? argv[1] : "lms.db";
  try {
    DatabaseConnection db(dbPath);
    SystemClock clock;
    LmsServices services(db, clock);
    prepareDatabase(db, LMS_SCHEMA_PATH, services.users, services.hasher, std::cout);
    ConsoleApp app(services, std::cin, std::cout);
    app.run();
  } catch (const std::exception& e) {
    std::cerr << "Cannot start: " << e.what() << "\n";
    return 1;
  }
  return 0;
}

#pragma once
#include <functional>
#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "lms/app/LmsServices.hpp"

namespace lms {

// The text menus for all four roles. It only reads input, calls the services and
// prints results; every rule lives in the services. Errors from a service are shown
// as a message and the menu carries on (NFR7). Reading from streams, not std::cin
// directly, lets the system tests drive a whole session from a script.
class ConsoleApp {
 public:
  ConsoleApp(LmsServices& services, std::istream& in, std::ostream& out)
      : s_(services), in_(in), out_(out) {}

  void run();  // login loop until the user types "quit" or input ends

 private:
  using Menu = std::vector<std::pair<std::string, std::function<void()>>>;

  void runMenu(const std::string& title, const Menu& menu);
  void attempt(const std::function<void()>& action);  // runs one option, shows any error

  // Input helpers. They throw EndOfInput when the input stream ends.
  std::string text(const std::string& prompt);
  int number(const std::string& prompt);
  TimePoint dateTime(const std::string& prompt);
  Role role(const std::string& prompt);

  std::string nameOf(int userId) const;
  void printMark(const MarkDetails& d);

  void adminMenu(const User& u);
  void lecturerMenu(const User& u);
  void hodMenu(const User& u);
  void studentMenu(const User& u);

  void adminListUsers(const User& u);
  void adminCreateUser(const User& u);
  void lecturerListAssignments(const User& u);
  void lecturerStatus(const User& u);
  void hodAwaiting(const User& u);
  void hodListAssignments(const User& u);
  void hodRelease(const User& u);
  void studentAssignments(const User& u);
  void studentResult(const User& u);
  void showAudit(const User& u);

  LmsServices& s_;
  std::istream& in_;
  std::ostream& out_;
};

}  // namespace lms

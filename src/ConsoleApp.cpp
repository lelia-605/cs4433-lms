#include "lms/ui/ConsoleApp.hpp"

#include <optional>

#include "lms/services/InputValidator.hpp"

namespace lms {

namespace {
// Thrown by the input helpers when the input ends (Ctrl+D, or the end of a test script).
struct EndOfInput {};

std::string stateText(SubmissionState s) {
  switch (s) {
    case SubmissionState::Submitted: return "submitted";
    case SubmissionState::Late:      return "late";
    case SubmissionState::Marked:    return "marked";
    case SubmissionState::Moderated: return "moderated";
    case SubmissionState::Released:  return "released";
  }
  return "?";
}

// Shows "2026-11-01 10:00" instead of the stored "2026-11-01T10:00:00Z".
std::string shortTime(TimePoint t) {
  const std::string iso = toIso(t);
  return iso.substr(0, 10) + " " + iso.substr(11, 5);
}

std::optional<int> optionalId(int value) { return value > 0 ? std::optional<int>(value) : std::nullopt; }
}  // namespace

// ---------------------------------------------------------------- input helpers

std::string ConsoleApp::text(const std::string& prompt) {
  out_ << prompt << ": " << std::flush;
  std::string line;
  if (!std::getline(in_, line)) throw EndOfInput{};
  if (!line.empty() && line.back() == '\r') line.pop_back();  // Windows line endings
  return line;
}

int ConsoleApp::number(const std::string& prompt) {
  while (true) {
    if (auto value = InputValidator::parseInt(text(prompt))) return *value;
    out_ << "Please type a whole number.\n";
  }
}

TimePoint ConsoleApp::dateTime(const std::string& prompt) {
  while (true) {
    if (auto t = InputValidator::parseDateTime(text(prompt + " (YYYY-MM-DD HH:MM, UTC)"))) return *t;
    out_ << "Please type a date and time like 2026-11-01 10:00.\n";
  }
}

Role ConsoleApp::role(const std::string& prompt) {
  while (true) {
    const int r = number(prompt + " (1 Administrator, 2 Lecturer, 3 Head of Department, 4 Student)");
    if (r == 1) return Role::Administrator;
    if (r == 2) return Role::Lecturer;
    if (r == 3) return Role::HeadOfDepartment;
    if (r == 4) return Role::Student;
    out_ << "Please choose 1 to 4.\n";
  }
}

std::string ConsoleApp::nameOf(int userId) const {
  auto u = s_.users.findById(userId);
  return u ? u->fullName() + " (" + u->username() + ")" : "user " + std::to_string(userId);
}

void ConsoleApp::printMark(const MarkDetails& d) {
  out_ << "  Raw mark:   " << d.rawMark << "/" << d.maxMark << "\n"
       << "  Days late:  " << d.daysLate << "\n"
       << "  Penalty:    -" << d.penalty << "\n"
       << "  Final mark: " << d.finalMark << "/" << d.maxMark << "\n"
       << "  Feedback:   " << d.feedback << "\n";
}

// ---------------------------------------------------------------- main loop

void ConsoleApp::attempt(const std::function<void()>& action) {
  try {
    action();
  } catch (const RuleViolation& e) {
    out_ << "Not done: " << e.what() << "\n";
  } catch (const AccessDenied& e) {
    out_ << "Access denied: " << e.what() << "\n";
  } catch (const NotFound& e) {
    out_ << "Not found: " << e.what() << "\n";
  } catch (const DatabaseError& e) {
    out_ << "Database problem: " << e.what() << "\n";
  }
}

void ConsoleApp::runMenu(const std::string& title, const Menu& menu) {
  while (true) {
    out_ << "\n== " << title << " ==\n";
    for (std::size_t i = 0; i < menu.size(); ++i) out_ << " " << i + 1 << ". " << menu[i].first << "\n";
    out_ << " 0. Log out\n";
    const int choice = number("Choose");
    if (choice == 0) return;
    if (choice < 0 || choice > static_cast<int>(menu.size())) {
      out_ << "No such option.\n";
      continue;
    }
    attempt(menu[static_cast<std::size_t>(choice - 1)].second);
  }
}

void ConsoleApp::run() {
  out_ << "Moderated Learning Management System\n";
  try {
    while (true) {
      out_ << "\n-- Log in (type quit as the username to exit) --\n";
      const std::string username = text("Username");
      if (username == "quit") break;
      const std::string password = text("Password");
      auto user = s_.auth.login(username, password);
      if (!user) {
        out_ << "Wrong username or password.\n";  // same message either way (FR1)
        continue;
      }
      out_ << "Welcome, " << user->fullName() << ".\n";
      switch (user->role()) {  // each role gets its own menu
        case Role::Administrator:    adminMenu(*user); break;
        case Role::Lecturer:         lecturerMenu(*user); break;
        case Role::HeadOfDepartment: hodMenu(*user); break;
        case Role::Student:          studentMenu(*user); break;
      }
      out_ << "Logged out.\n";
    }
  } catch (const EndOfInput&) {
    out_ << "\n";
  }
  out_ << "Goodbye.\n";
}

// ---------------------------------------------------------------- administrator

void ConsoleApp::adminListUsers(const User& u) {
  for (const auto& user : s_.admin.listUsers(u))
    out_ << "  " << user->id() << "  " << user->username() << "  " << user->fullName() << "  "
         << roleToString(user->role()) << (user->active() ? "" : "  (deactivated)") << "\n";
}

void ConsoleApp::adminCreateUser(const User& u) {
  const std::string username = text("Username");
  const std::string password = text("Password (at least 8 characters)");
  const std::string fullName = text("Full name");
  const Role r = role("Role");
  std::optional<int> dept;
  if (r == Role::Lecturer || r == Role::HeadOfDepartment) dept = number("Department id");
  const int id = s_.admin.createUser(u, username, password, fullName, r, dept);
  out_ << "User created with id " << id << ".\n";
}

void ConsoleApp::adminMenu(const User& u) {
  runMenu("Administrator", {
    {"List users", [&] { adminListUsers(u); }},
    {"Create user", [&] { adminCreateUser(u); }},
    {"Edit user", [&] {
      const int id = number("User id");
      const std::string name = text("New full name");
      s_.admin.editUser(u, id, name, optionalId(number("Department id (0 for none)")));
      out_ << "User updated.\n";
    }},
    {"Deactivate user", [&] { s_.admin.deactivateUser(u, number("User id")); out_ << "User deactivated.\n"; }},
    {"Reset a password", [&] {
      const int id = number("User id");
      s_.admin.resetPassword(u, id, text("New password (at least 8 characters)"));
      out_ << "Password reset.\n";
    }},
    {"List departments", [&] {
      for (const auto& d : s_.admin.listDepartments(u)) out_ << "  " << d.id << "  " << d.name << "\n";
    }},
    {"Create department", [&] {
      out_ << "Department created with id " << s_.admin.createDepartment(u, text("Name")) << ".\n";
    }},
    {"List courses", [&] {
      for (const auto& c : s_.admin.listCourses(u))
        out_ << "  " << c.id << "  " << c.code << "  " << c.name << "  dept " << c.departmentId
             << "  lecturer: " << (c.lecturerId ? nameOf(*c.lecturerId) : "none") << "\n";
    }},
    {"Create course", [&] {
      const std::string code = text("Course code");
      const std::string name = text("Course name");
      out_ << "Course created with id " << s_.admin.createCourse(u, code, name, number("Department id"))
           << ".\n";
    }},
    {"Rename course", [&] {
      const int id = number("Course id");
      s_.admin.renameCourse(u, id, text("New name"));
      out_ << "Course renamed.\n";
    }},
    {"Assign lecturer to course", [&] {
      const int course = number("Course id");
      s_.admin.assignLecturer(u, course, number("Lecturer user id"));
      out_ << "Lecturer assigned.\n";
    }},
    {"Enrol student in course", [&] {
      const int student = number("Student user id");
      s_.admin.enrolStudent(u, student, number("Course id"));
      out_ << "Student enrolled.\n";
    }},
    {"Rename department", [&] {
      const int id = number("Department id");
      s_.admin.renameDepartment(u, id, text("New name"));
      out_ << "Department renamed.\n";
    }},
    {"Audit trail for a submission", [&] { showAudit(u); }},
  });
}

// ---------------------------------------------------------------- lecturer

void ConsoleApp::lecturerListAssignments(const User& u) {
  for (const auto& a : s_.assignments.assignmentsForLecturer(u, number("Course id")))
    out_ << "  " << a.id << "  " << a.title << "  due " << shortTime(a.deadline) << "  out of "
         << a.maxMark << "\n";
}

void ConsoleApp::lecturerStatus(const User& u) {
  for (const auto& row : s_.submissions.statusForAssignment(u, number("Assignment id"))) {
    out_ << "  " << row.studentId << "  " << nameOf(row.studentId) << "  ";
    if (!row.submitted) {
      out_ << "not submitted\n";
      continue;
    }
    const Submission& sub = *row.submission;
    out_ << stateText(sub.state) << ", " << shortTime(sub.submittedAt) << ", " << sub.daysLate
         << " day(s) late, " << row.versionCount << " version(s)";
    if (sub.rawMark) out_ << ", raw mark " << *sub.rawMark;
    out_ << "\n";
  }
}

void ConsoleApp::showAudit(const User& u) {
  const int assignment = number("Assignment id");
  for (const auto& e : s_.marking.auditTrail(u, assignment, number("Student user id"))) {
    out_ << "  " << shortTime(e.changedAt) << "  " << nameOf(e.changedBy) << "  "
         << (e.oldValue ? std::to_string(*e.oldValue) : "-") << " -> "
         << (e.newValue ? std::to_string(*e.newValue) : "-") << "  reason: " << e.reason << "\n";
  }
}

void ConsoleApp::lecturerMenu(const User& u) {
  runMenu("Lecturer", {
    {"My courses", [&] {
      for (const auto& c : s_.assignments.coursesForLecturer(u))
        out_ << "  " << c.id << "  " << c.code << "  " << c.name << "\n";
    }},
    {"Assignments in a course", [&] { lecturerListAssignments(u); }},
    {"Create assignment", [&] {
      const int course = number("Course id");
      const std::string title = text("Title");
      const TimePoint deadline = dateTime("Deadline");
      const int id = s_.assignments.createAssignment(u, course, title, deadline, number("Maximum mark"));
      out_ << "Assignment created with id " << id << ".\n";
    }},
    {"Grant an extension", [&] {
      const int assignment = number("Assignment id");
      const int student = number("Student user id");
      const TimePoint until = dateTime("New deadline");
      s_.assignments.grantExtension(u, assignment, student, until, text("Reason"));
      out_ << "Extension granted.\n";
    }},
    {"Submission status", [&] { lecturerStatus(u); }},
    {"Mark a submission", [&] {
      const int assignment = number("Assignment id");
      const int student = number("Student user id");
      const int mark = number("Mark");
      s_.marking.markSubmission(u, assignment, student, mark, text("Feedback"));
      printMark(s_.marking.markDetails(u, assignment, student));
    }},
    {"Change a mark", [&] {
      const int assignment = number("Assignment id");
      const int student = number("Student user id");
      const int mark = number("New mark");
      const std::string feedback = text("Feedback");
      s_.marking.changeMark(u, assignment, student, mark, feedback, text("Reason for the change"));
      printMark(s_.marking.markDetails(u, assignment, student));
    }},
    {"View a mark", [&] {
      const int assignment = number("Assignment id");
      printMark(s_.marking.markDetails(u, assignment, number("Student user id")));
    }},
    {"Audit trail for a submission", [&] { showAudit(u); }},
  });
}

// ---------------------------------------------------------------- head of department

void ConsoleApp::hodAwaiting(const User& u) {
  const auto items = s_.moderation.listForModeration(u);
  if (items.empty()) out_ << "  Nothing is waiting for moderation.\n";
  for (const auto& item : items) {
    out_ << "  Submission " << item.submission.id << "  " << item.assignment.title << "  "
         << nameOf(item.submission.studentId) << "  raw " << item.details.rawMark << ", final "
         << item.details.finalMark << "/" << item.details.maxMark << " (" << item.details.daysLate
         << " day(s) late, penalty -" << item.details.penalty << ")\n"
         << "      marked by " << (item.lecturerId ? nameOf(*item.lecturerId) : "-")
         << (item.extension ? ", extension to " + shortTime(item.extension->newDeadline) : ", no extension")
         << "\n";
  }
}

void ConsoleApp::hodListAssignments(const User& u) {
  for (const auto& a : s_.moderation.departmentAssignments(u))
    out_ << "  " << a.id << "  " << a.title << "  course " << a.courseId << "  due "
         << shortTime(a.deadline) << "\n";
}

void ConsoleApp::hodRelease(const User& u) {
  const int assignment = number("Assignment id");
  int released = 0;
  try {
    released = s_.moderation.release(u, assignment);
  } catch (const RuleViolation& e) {  // some work is not moderated yet (UC09 E3)
    out_ << e.what() << "\n";
    if (text("Release only the moderated marks? (y/n)") != "y") return;
    released = s_.moderation.release(u, assignment, true);  // partial release (UC09 A3)
  }
  out_ << released << " mark(s) released.\n";
}

void ConsoleApp::hodMenu(const User& u) {
  runMenu("Head of Department", {
    {"Marks waiting for moderation", [&] { hodAwaiting(u); }},
    {"Approve a mark", [&] { s_.moderation.approve(u, number("Submission id")); out_ << "Mark approved.\n"; }},
    {"Adjust a mark", [&] {
      const int id = number("Submission id");
      const int mark = number("New mark");
      s_.moderation.adjustMark(u, id, mark, text("Reason for the adjustment"));
      out_ << "Mark adjusted.\n";
    }},
    {"Approve all marks for an assignment", [&] {
      out_ << s_.moderation.approveAll(u, number("Assignment id")) << " mark(s) approved.\n";
    }},
    {"Assignments in my department", [&] { hodListAssignments(u); }},
    {"Release marks for an assignment", [&] { hodRelease(u); }},
    {"Audit trail for a submission", [&] { showAudit(u); }},
  });
}

// ---------------------------------------------------------------- student

void ConsoleApp::studentAssignments(const User& u) {
  const int course = number("Course id");
  for (const auto& v : s_.assignments.assignmentsForStudent(u, course)) {
    out_ << "  " << v.assignment.id << "  " << v.assignment.title << "  due "
         << shortTime(v.effectiveDeadline) << (v.hasExtension ? " (extended)" : "") << "  ";
    auto mine = s_.submissions.mySubmission(u, v.assignment.id);
    out_ << (mine ? "you submitted " + shortTime(mine->submittedAt) : std::string("not submitted")) << "\n";
  }
}

void ConsoleApp::studentResult(const User& u) {
  const StudentResult r = s_.moderation.resultFor(u, number("Assignment id"));
  if (!r.submitted) {
    out_ << "  You have not submitted this assignment.\n";
  } else if (!r.released) {
    out_ << "  Submitted. Your mark has not been released yet.\n";  // hidden until release (BR4)
  } else {
    printMark(*r.details);
  }
}

void ConsoleApp::studentMenu(const User& u) {
  runMenu("Student", {
    {"My courses", [&] {
      for (const auto& c : s_.assignments.coursesForStudent(u))
        out_ << "  " << c.id << "  " << c.code << "  " << c.name << "\n";
    }},
    {"Assignments in a course", [&] { studentAssignments(u); }},
    {"Submit or resubmit", [&] {
      const int assignment = number("Assignment id");
      const int version = s_.submissions.submit(u, assignment, text("Your answer (one line)"));
      const auto mine = s_.submissions.mySubmission(u, assignment);
      out_ << "Saved as version " << version << ".";
      if (mine && mine->daysLate > 0) out_ << " This is " << mine->daysLate << " day(s) late.";
      out_ << "\n";
    }},
    {"My submitted versions", [&] {
      for (const auto& v : s_.submissions.myVersions(u, number("Assignment id")))
        out_ << "  v" << v.versionNo << "  " << shortTime(v.submittedAt) << "  " << v.content << "\n";
    }},
    {"My result", [&] { studentResult(u); }},
  });
}

}  // namespace lms

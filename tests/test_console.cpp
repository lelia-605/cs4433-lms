// System tests: whole sessions typed into the console menus, as in the use cases.
#include <gtest/gtest.h>

#include <cstdio>
#include <sstream>

#include "lms/app/DatabaseSetup.hpp"
#include "lms/ui/ConsoleApp.hpp"

using namespace lms;

namespace {
// Joins script lines with newlines, as if typed at the keyboard.
std::string script(std::initializer_list<const char*> lines) {
  std::string s;
  for (const char* l : lines) s += std::string(l) + "\n";
  return s;
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}
}  // namespace

class ConsoleFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  LmsServices services{db, clock};
  std::ostringstream setupOut;

  void SetUp() override {
    prepareDatabase(db, LMS_SCHEMA_PATH, services.users, services.hasher, setupOut);
  }

  std::string run(const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    ConsoleApp(services, in, out).run();
    return out.str();
  }

  // Admin sets up: department 1, lecturer 2, HoD 3, student 4, course 1.
  std::string adminSetup() {
    return script({"admin", "admin12345",
                   "7", "Science",
                   "2", "lee", "password1", "Lee Lecturer", "2", "1",
                   "2", "hal", "password1", "Hal Head", "3", "1",
                   "2", "sam", "password1", "Sam Student", "4",
                   "9", "CS101", "Intro to Computing", "1",
                   "11", "1", "2",
                   "12", "4", "1",
                   "0"});
  }
};

TEST_F(ConsoleFixture, FirstStartCreatesSchemaAndAdmin) {
  EXPECT_TRUE(contains(setupOut.str(), "New database created"));
  EXPECT_TRUE(contains(setupOut.str(), "First administrator created"));
  EXPECT_TRUE(services.auth.login(kFirstAdminUsername, kFirstAdminPassword) != nullptr);
}

TEST_F(ConsoleFixture, WrongPasswordGivesOneGenericMessage) {
  const std::string out = run(script({"admin", "wrong", "nobody", "x", "quit"}));
  // wrong password and unknown user get exactly the same message, twice
  std::size_t count = 0;
  for (auto pos = out.find("Wrong username or password."); pos != std::string::npos;
       pos = out.find("Wrong username or password.", pos + 1))
    ++count;
  EXPECT_EQ(count, 2u);
  EXPECT_FALSE(contains(out, "Welcome"));
  EXPECT_TRUE(contains(out, "Goodbye."));
}

// Use cases: set up a course, create an assignment, submit, mark, moderate, release, view result.
TEST_F(ConsoleFixture, WholeTermFromSetupToReleasedMark) {
  std::string out = run(adminSetup() + script({"quit"}));
  EXPECT_TRUE(contains(out, "User created with id 4."));
  EXPECT_TRUE(contains(out, "Student enrolled."));

  out = run(script({"lee", "password1", "3", "1", "Essay 1", "2026-11-01 10:00", "50", "0", "quit"}));
  EXPECT_TRUE(contains(out, "Assignment created with id 1."));

  out = run(script({"sam", "password1", "3", "1", "My essay text", "5", "1", "0", "quit"}));
  EXPECT_TRUE(contains(out, "Saved as version 1."));
  EXPECT_TRUE(contains(out, "Your mark has not been released yet."));

  out = run(script({"lee", "password1", "6", "1", "4", "40", "Good work", "0", "quit"}));
  EXPECT_TRUE(contains(out, "Final mark: 40/50"));

  out = run(script({"hal", "password1", "1", "2", "1", "6", "1", "0", "quit"}));
  EXPECT_TRUE(contains(out, "Submission 1  Essay 1  Sam Student (sam)  raw 40, final 40/50"));
  EXPECT_TRUE(contains(out, "marked by Lee Lecturer (lee), no extension"));  // FR15
  EXPECT_TRUE(contains(out, "Mark approved."));
  EXPECT_TRUE(contains(out, "1 mark(s) released."));

  out = run(script({"sam", "password1", "5", "1", "0", "quit"}));
  EXPECT_TRUE(contains(out, "Final mark: 40/50"));
  EXPECT_TRUE(contains(out, "Feedback:   Good work"));
}

TEST_F(ConsoleFixture, LateSubmissionIsAcceptedAndPenalised) {
  run(adminSetup());
  run(script({"lee", "password1", "3", "1", "Essay 1", "2026-11-01 10:00", "50", "0"}));
  clock.set(fromIso("2026-11-02T12:00:00Z"));  // 2 days late
  std::string out = run(script({"sam", "password1", "3", "1", "late essay", "0"}));
  EXPECT_TRUE(contains(out, "This is 2 day(s) late."));
  out = run(script({"lee", "password1", "6", "1", "4", "40", "ok", "0"}));
  EXPECT_TRUE(contains(out, "Penalty:    -10"));
  EXPECT_TRUE(contains(out, "Final mark: 30/50"));
}

TEST_F(ConsoleFixture, BadInputAndRuleErrorsDoNotStopTheProgram) {
  run(adminSetup());
  const std::string out = run(script({"sam", "password1",
                                      "abc",          // not a number
                                      "99",           // no such option
                                      "3", "77", "x",  // assignment 77 does not exist
                                      "0", "lee", "password1",
                                      "3", "1", "Essay", "1 Nov", "2026-11-01 10:00", "5000",  // bad date, bad max
                                      "0", "quit"}));
  EXPECT_TRUE(contains(out, "Please type a whole number."));
  EXPECT_TRUE(contains(out, "No such option."));
  EXPECT_TRUE(contains(out, "Not found: assignment not found"));
  EXPECT_TRUE(contains(out, "Please type a date and time like 2026-11-01 10:00."));
  EXPECT_TRUE(contains(out, "Not done: maximum mark must be between 1 and 1000"));
  EXPECT_TRUE(contains(out, "Goodbye."));
}

TEST_F(ConsoleFixture, RolesOnlySeeTheirOwnMenu) {
  run(adminSetup());
  EXPECT_TRUE(contains(run(script({"sam", "password1", "0"})), "== Student =="));
  EXPECT_TRUE(contains(run(script({"lee", "password1", "0"})), "== Lecturer =="));
  EXPECT_TRUE(contains(run(script({"hal", "password1", "0"})), "== Head of Department =="));
  const std::string admin = run(script({"admin", "admin12345", "0"}));
  EXPECT_TRUE(contains(admin, "== Administrator =="));
  EXPECT_FALSE(contains(admin, "Mark a submission"));
}

TEST_F(ConsoleFixture, EndOfInputInsideAMenuExitsCleanly) {
  const std::string out = run(script({"admin", "admin12345", "2", "newuser"}));  // input stops mid-form
  EXPECT_TRUE(contains(out, "Goodbye."));
}

// NFR3: data is kept in the database file between runs.
TEST(Persistence, DataSurvivesRestartAndAdminIsNotRecreated) {
  const std::string path = "lms_persistence_test.db";
  std::remove(path.c_str());
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  {
    DatabaseConnection db(path);
    LmsServices s(db, clock);
    std::ostringstream out;
    prepareDatabase(db, LMS_SCHEMA_PATH, s.users, s.hasher, out);
    auto admin = s.auth.login(kFirstAdminUsername, kFirstAdminPassword);
    s.admin.createDepartment(*admin, "Science");
  }
  {
    DatabaseConnection db(path);
    LmsServices s(db, clock);
    std::ostringstream out;
    prepareDatabase(db, LMS_SCHEMA_PATH, s.users, s.hasher, out);
    EXPECT_EQ(out.str(), "");  // nothing created the second time
    auto admin = s.auth.login(kFirstAdminUsername, kFirstAdminPassword);
    ASSERT_TRUE(admin != nullptr);
    auto depts = s.admin.listDepartments(*admin);
    ASSERT_EQ(depts.size(), 1u);
    EXPECT_EQ(depts[0].name, "Science");
    EXPECT_EQ(s.admin.listUsers(*admin).size(), 1u);
  }
  std::remove(path.c_str());
}

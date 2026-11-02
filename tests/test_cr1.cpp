// CR1 (release v1.1): class-wide deadline extensions, shown on the student's assignment list.
#include <gtest/gtest.h>

#include <cstdio>
#include <sstream>

#include "lms/app/DatabaseSetup.hpp"
#include "lms/ui/ConsoleApp.hpp"

using namespace lms;
using namespace std::chrono;

class Cr1Fixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  LmsServices s{db, clock};
  Administrator root{1000, "root", "x", "Root", true};
  const TimePoint deadline = fromIso("2026-11-01T10:00:00Z");
  int course = 0, essay = 0;
  std::unique_ptr<User> lee, olga, hod, sam, tom;

  std::unique_ptr<User> make(const std::string& name, Role role, std::optional<int> d = std::nullopt) {
    return s.users.findById(s.admin.createUser(root, name, "password1", name, role, d));
  }

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    const int dept = s.admin.createDepartment(root, "Science");
    course = s.admin.createCourse(root, "CS101", "Intro", dept);
    lee = make("lee", Role::Lecturer, dept);
    olga = make("olga", Role::Lecturer, dept);
    hod = make("hal", Role::HeadOfDepartment, dept);
    sam = make("sam", Role::Student);
    tom = make("tom", Role::Student);
    s.admin.assignLecturer(root, course, lee->id());
    s.admin.enrolStudent(root, sam->id(), course);
    s.admin.enrolStudent(root, tom->id(), course);
    essay = s.assignments.createAssignment(*lee, course, "Essay 1", deadline, 50);
  }

  void extendClass(const char* until) {
    s.assignments.grantClassExtension(*lee, essay, fromIso(until), "school trip");
  }
};

// ---- the new rule: every student gets the class deadline ----
TEST_F(Cr1Fixture, ClassExtensionMovesEveryStudentsDeadline) {
  extendClass("2026-11-04T10:00:00Z");
  EXPECT_EQ(toIso(s.assignments.effectiveDeadline(essay, sam->id())), "2026-11-04T10:00:00Z");
  EXPECT_EQ(toIso(s.assignments.effectiveDeadline(essay, tom->id())), "2026-11-04T10:00:00Z");
  EXPECT_EQ(toIso(s.assignmentsRepo.findAssignment(essay)->deadline), "2026-11-01T10:00:00Z");  // original kept
  auto stored = s.assignmentsRepo.findClassExtension(essay);
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->grantedBy, lee->id());
  EXPECT_EQ(stored->reason, "school trip");
}

TEST_F(Cr1Fixture, StudentEnrolledLaterAlsoGetsTheClassDeadline) {
  extendClass("2026-11-04T10:00:00Z");
  auto ann = make("ann", Role::Student);
  s.admin.enrolStudent(root, ann->id(), course);
  EXPECT_EQ(toIso(s.assignments.effectiveDeadline(essay, ann->id())), "2026-11-04T10:00:00Z");
}

TEST_F(Cr1Fixture, LaterOfClassAndIndividualExtensionApplies) {
  s.assignments.grantExtension(*lee, essay, sam->id(), fromIso("2026-11-06T10:00:00Z"), "illness");
  extendClass("2026-11-04T10:00:00Z");
  EXPECT_EQ(toIso(s.assignments.effectiveDeadline(essay, sam->id())), "2026-11-06T10:00:00Z");  // own is later
  EXPECT_EQ(s.assignments.extensionKind(essay, sam->id()), ExtensionKind::Individual);
  extendClass("2026-11-08T10:00:00Z");
  EXPECT_EQ(toIso(s.assignments.effectiveDeadline(essay, sam->id())), "2026-11-08T10:00:00Z");  // class is later
  EXPECT_EQ(s.assignments.extensionKind(essay, sam->id()), ExtensionKind::Class);
  EXPECT_EQ(s.assignments.extensionKind(essay, tom->id()), ExtensionKind::Class);
}

TEST_F(Cr1Fixture, IndividualExtensionMustBeatTheClassDeadline) {
  extendClass("2026-11-04T10:00:00Z");
  EXPECT_THROW(s.assignments.grantExtension(*lee, essay, sam->id(), fromIso("2026-11-03T10:00:00Z"), "r"),
               RuleViolation);
  EXPECT_NO_THROW(s.assignments.grantExtension(*lee, essay, sam->id(), fromIso("2026-11-05T10:00:00Z"), "r"));
}

// ---- rules for a class extension ----
TEST_F(Cr1Fixture, ClassExtensionRules) {
  const TimePoint later = fromIso("2026-11-04T10:00:00Z");
  EXPECT_THROW(s.assignments.grantClassExtension(*olga, essay, later, "r"), AccessDenied);  // not their course
  EXPECT_THROW(s.assignments.grantClassExtension(*hod, essay, later, "r"), AccessDenied);
  EXPECT_THROW(s.assignments.grantClassExtension(*sam, essay, later, "r"), AccessDenied);
  EXPECT_THROW(s.assignments.grantClassExtension(*lee, 999, later, "r"), NotFound);
  EXPECT_THROW(s.assignments.grantClassExtension(*lee, essay, later, "  "), RuleViolation);  // no reason
  EXPECT_THROW(s.assignments.grantClassExtension(*lee, essay, deadline, "r"), RuleViolation);  // boundary: equal
  EXPECT_THROW(s.assignments.grantClassExtension(*lee, essay, deadline - hours(1), "r"), RuleViolation);
  EXPECT_NO_THROW(s.assignments.grantClassExtension(*lee, essay, deadline + seconds(1), "r"));  // boundary: +1 s
}

TEST_F(Cr1Fixture, SecondClassExtensionReplacesFirstAndMustBeLater) {
  extendClass("2026-11-04T10:00:00Z");
  EXPECT_THROW(extendClass("2026-11-03T10:00:00Z"), RuleViolation);
  extendClass("2026-11-05T10:00:00Z");
  EXPECT_EQ(toIso(s.assignments.classDeadline(essay)), "2026-11-05T10:00:00Z");
}

TEST_F(Cr1Fixture, ClassExtensionAfterTheDeadlineMustBeInTheFuture) {
  clock.set(fromIso("2026-11-02T10:00:00Z"));  // original deadline has passed
  EXPECT_THROW(extendClass("2026-11-02T09:00:00Z"), RuleViolation);
  EXPECT_NO_THROW(extendClass("2026-11-03T10:00:00Z"));
}

// ---- lateness uses the class deadline ----
TEST_F(Cr1Fixture, LatenessCountsFromTheClassDeadline) {
  extendClass("2026-11-04T10:00:00Z");
  clock.set(fromIso("2026-11-04T10:00:00Z"));  // boundary: exactly on the class deadline
  s.submissions.submit(*sam, essay, "on time");
  clock.set(fromIso("2026-11-04T10:00:01Z"));  // boundary: one second after
  s.submissions.submit(*tom, essay, "late");
  EXPECT_EQ(s.submissionsRepo.find(essay, sam->id())->state, SubmissionState::Submitted);
  EXPECT_EQ(s.submissionsRepo.find(essay, tom->id())->daysLate, 1);
}

TEST_F(Cr1Fixture, ExtensionRecalculatesUnmarkedLateWork) {
  clock.set(deadline + hours(30));  // both hand in 2 days late
  s.submissions.submit(*sam, essay, "late");
  s.submissions.submit(*tom, essay, "late");
  s.marking.markSubmission(*lee, essay, tom->id(), 40, "ok");  // tom is already marked
  extendClass("2026-11-05T10:00:00Z");
  EXPECT_EQ(s.submissions.refreshLateness(*lee, essay), 1);
  auto samSub = s.submissionsRepo.find(essay, sam->id());
  EXPECT_EQ(samSub->state, SubmissionState::Submitted);
  EXPECT_EQ(samSub->daysLate, 0);
  auto tomSub = s.submissionsRepo.find(essay, tom->id());
  EXPECT_EQ(tomSub->state, SubmissionState::Marked);  // marked work is never changed
  EXPECT_EQ(tomSub->daysLate, 2);
  EXPECT_EQ(s.submissions.refreshLateness(*lee, essay), 0);  // nothing left to change
  EXPECT_THROW(s.submissions.refreshLateness(*olga, essay), AccessDenied);
}

// ---- the extension is shown to everyone who needs it ----
TEST_F(Cr1Fixture, StudentListShowsKindOfExtensionAndOriginalDeadline) {
  s.assignments.createAssignment(*lee, course, "Essay 2", fromIso("2026-11-10T10:00:00Z"), 20);
  extendClass("2026-11-04T10:00:00Z");
  s.assignments.grantExtension(*lee, essay, sam->id(), fromIso("2026-11-06T10:00:00Z"), "illness");
  auto samView = s.assignments.assignmentsForStudent(*sam, course);
  auto tomView = s.assignments.assignmentsForStudent(*tom, course);
  EXPECT_EQ(samView[0].extensionKind, ExtensionKind::Individual);
  EXPECT_EQ(toIso(samView[0].effectiveDeadline), "2026-11-06T10:00:00Z");
  EXPECT_EQ(tomView[0].extensionKind, ExtensionKind::Class);
  EXPECT_EQ(toIso(tomView[0].effectiveDeadline), "2026-11-04T10:00:00Z");
  EXPECT_EQ(toIso(tomView[0].assignment.deadline), "2026-11-01T10:00:00Z");  // original still shown
  EXPECT_EQ(tomView[1].extensionKind, ExtensionKind::None);
  EXPECT_FALSE(tomView[1].hasExtension);
}

TEST_F(Cr1Fixture, HodSeesTheClassExtensionWhenModerating) {
  extendClass("2026-11-04T10:00:00Z");
  s.submissions.submit(*sam, essay, "answer");
  s.marking.markSubmission(*lee, essay, sam->id(), 40, "ok");
  auto items = s.moderation.listForModeration(*hod);
  ASSERT_EQ(items.size(), 1u);
  ASSERT_TRUE(items[0].classExtension.has_value());
  EXPECT_EQ(toIso(items[0].classExtension->newDeadline), "2026-11-04T10:00:00Z");
}

// ---- whole sessions through the console ----
namespace {
std::string session(LmsServices& services, std::initializer_list<const char*> lines) {
  std::string input;
  for (const char* l : lines) input += std::string(l) + "\n";
  std::istringstream in(input);
  std::ostringstream out;
  ConsoleApp(services, in, out).run();
  return out.str();
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
}  // namespace

TEST_F(Cr1Fixture, LecturerExtendsWholeClassAndStudentsSeeIt) {
  std::string out = session(s, {"lee", "password1", "10", "1", "2026-11-04 10:00", "school trip", "0", "quit"});
  EXPECT_TRUE(has(out, "Deadline extended for the whole class."));
  out = session(s, {"lee", "password1", "2", "1", "0", "quit"});
  EXPECT_TRUE(has(out, "due 2026-11-04 10:00 (class extension; original 2026-11-01 10:00)"));
  out = session(s, {"tom", "password1", "2", "1", "0", "quit"});
  EXPECT_TRUE(has(out, "due 2026-11-04 10:00 (extended for the whole class; original 2026-11-01 10:00)"));
}

TEST_F(Cr1Fixture, ConsoleWarnsAboutZeroMarkAndShowsWorkBeforeMarking) {  // defects D13 and D14
  clock.set(deadline + hours(100));  // more than 3 days late
  std::string out = session(s, {"sam", "password1", "3", "1", "my very late essay", "0", "quit"});
  EXPECT_TRUE(has(out, "This is 5 day(s) late. Work more than 3 days late receives a mark of zero."));
  out = session(s, {"lee", "password1", "6", "1", std::to_string(sam->id()).c_str(), "40", "ok", "0", "quit"});
  EXPECT_TRUE(has(out, "Submitted work (version 1"));
  EXPECT_TRUE(has(out, "my very late essay"));
  EXPECT_TRUE(has(out, "Final mark: 0/50"));
}

// ---- a database made by v1.0 is upgraded and keeps its data ----
TEST(Cr1Upgrade, Version10DatabaseIsUpgradedWithoutLosingData) {
  const std::string path = "lms_upgrade_test.db";
  std::remove(path.c_str());
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  {
    DatabaseConnection db(path);
    LmsServices s(db, clock);
    std::ostringstream out;
    prepareDatabase(db, LMS_SCHEMA_PATH, s.users, s.hasher, out);
    db.exec("DROP TABLE class_extension");  // now it looks like a v1.0 database
    auto admin = s.auth.login(kFirstAdminUsername, kFirstAdminPassword);
    s.admin.createDepartment(*admin, "Science");
  }
  {
    DatabaseConnection db(path);
    LmsServices s(db, clock);
    std::ostringstream out;
    prepareDatabase(db, LMS_SCHEMA_PATH, s.users, s.hasher, out);
    EXPECT_NE(out.str().find("Database upgraded to version 1.1."), std::string::npos);
    EXPECT_TRUE(db.hasTable("class_extension"));
    auto admin = s.auth.login(kFirstAdminUsername, kFirstAdminPassword);
    EXPECT_EQ(s.admin.listDepartments(*admin).size(), 1u);  // old data still there
    std::ostringstream again;
    prepareDatabase(db, LMS_SCHEMA_PATH, s.users, s.hasher, again);
    EXPECT_EQ(again.str(), "");  // upgrading twice changes nothing
  }
  std::remove(path.c_str());
}

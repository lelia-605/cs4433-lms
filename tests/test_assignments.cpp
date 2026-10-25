#include <gtest/gtest.h>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteAssignmentRepository.hpp"
#include "lms/data/SqliteCourseRepository.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AdminService.hpp"
#include "lms/services/AssignmentService.hpp"
#include "lms/services/LateDays.hpp"

using namespace lms;
using namespace std::chrono;

// ---- ISO date and time helpers ----
TEST(Time, RoundTripsAndHandlesLeapYears) {
  EXPECT_EQ(toIso(TimePoint{}), "1970-01-01T00:00:00Z");
  for (const char* s : {"2026-11-01T10:00:00Z", "2024-02-29T23:59:59Z", "2000-01-01T00:00:00Z", "2100-03-01T12:30:15Z"})
    EXPECT_EQ(toIso(fromIso(s)), s);
  EXPECT_EQ(fromIso("2026-11-02T10:00:00Z") - fromIso("2026-11-01T10:00:00Z"), hours(24));
}

TEST(Time, RejectsInvalidDates) {
  for (const char* s : {"", "tomorrow", "2026-02-30T10:00:00Z", "2026-13-01T10:00:00Z", "2026-11-01 10:00:00",
                        "2026-11-01T25:00:00Z", "2023-02-29T00:00:00Z"})
    EXPECT_THROW(fromIso(s), RuleViolation) << s;
}

class AssignmentFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  LibsodiumHasher hasher;
  SqliteUserRepository users{db};
  SqliteCourseRepository courses{db};
  SqliteAssignmentRepository assignmentsRepo{db};
  AdminService admin{users, courses, hasher};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  AssignmentService service{assignmentsRepo, courses, clock};
  Administrator root{1000, "root", "x", "Root", true};

  int dept = 0, course = 0;
  std::unique_ptr<User> lee, otherLecturer, hod, sam, tom, outsider;

  std::unique_ptr<User> make(const std::string& name, Role role, std::optional<int> d = std::nullopt) {
    return users.findById(admin.createUser(root, name, "password1", name, role, d));
  }

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    dept = admin.createDepartment(root, "Science");
    course = admin.createCourse(root, "CS101", "Intro", dept);
    lee = make("lee", Role::Lecturer, dept);
    otherLecturer = make("olga", Role::Lecturer, dept);
    hod = make("hal", Role::HeadOfDepartment, dept);
    sam = make("sam", Role::Student);
    tom = make("tom", Role::Student);
    outsider = make("zed", Role::Student);
    admin.assignLecturer(root, course, lee->id());
    admin.enrolStudent(root, sam->id(), course);
    admin.enrolStudent(root, tom->id(), course);
  }

  int newAssignment(const char* deadline = "2026-11-01T10:00:00Z", int max = 50) {
    return service.createAssignment(*lee, course, "Essay 1", fromIso(deadline), max);
  }
};

// ---- FR6 ----
TEST_F(AssignmentFixture, LecturerCreatesAssignmentForOwnCourse) {
  const int id = newAssignment();
  auto a = assignmentsRepo.findAssignment(id);
  ASSERT_TRUE(a.has_value());
  EXPECT_EQ(a->title, "Essay 1");
  EXPECT_EQ(a->maxMark, 50);
  EXPECT_EQ(toIso(a->deadline), "2026-11-01T10:00:00Z");
}

TEST_F(AssignmentFixture, OnlyTheCoursesLecturerMayCreateAssignments) {
  const TimePoint d = fromIso("2026-11-01T10:00:00Z");
  EXPECT_THROW(service.createAssignment(*otherLecturer, course, "X", d, 10), AccessDenied);  // not their course
  EXPECT_THROW(service.createAssignment(*hod, course, "X", d, 10), AccessDenied);            // HoD cannot
  EXPECT_THROW(service.createAssignment(*sam, course, "X", d, 10), AccessDenied);
  EXPECT_THROW(service.createAssignment(*lee, 999, "X", d, 10), NotFound);
}

TEST_F(AssignmentFixture, RejectsInvalidAssignmentInput) {
  const TimePoint future = fromIso("2026-11-01T10:00:00Z");
  EXPECT_THROW(service.createAssignment(*lee, course, "  ", future, 10), RuleViolation);
  EXPECT_THROW(service.createAssignment(*lee, course, std::string(201, 'a'), future, 10), RuleViolation);
  EXPECT_THROW(service.createAssignment(*lee, course, "X", future, 0), RuleViolation);     // boundary: 0
  EXPECT_THROW(service.createAssignment(*lee, course, "X", future, 1001), RuleViolation);  // boundary: 1001
  EXPECT_NO_THROW(service.createAssignment(*lee, course, "X", future, 1));                 // boundary: 1
  EXPECT_NO_THROW(service.createAssignment(*lee, course, "Y", future, 1000));              // boundary: 1000
  EXPECT_THROW(service.createAssignment(*lee, course, "X", clock.now(), 10), RuleViolation);  // not in the future
  EXPECT_THROW(service.createAssignment(*lee, course, "X", clock.now() - hours(1), 10), RuleViolation);
}

// ---- FR7 ----
TEST_F(AssignmentFixture, ExtensionChangesOnlyThatStudentsDeadline) {
  const int a = newAssignment();
  service.grantExtension(*lee, a, sam->id(), fromIso("2026-11-03T10:00:00Z"), "illness");
  EXPECT_EQ(toIso(service.effectiveDeadline(a, sam->id())), "2026-11-03T10:00:00Z");
  EXPECT_EQ(toIso(service.effectiveDeadline(a, tom->id())), "2026-11-01T10:00:00Z");
}

TEST_F(AssignmentFixture, ExtensionRulesAreEnforced) {
  const int a = newAssignment();
  const TimePoint later = fromIso("2026-11-03T10:00:00Z");
  EXPECT_THROW(service.grantExtension(*otherLecturer, a, sam->id(), later, "r"), AccessDenied);
  EXPECT_THROW(service.grantExtension(*sam, a, tom->id(), later, "r"), AccessDenied);
  EXPECT_THROW(service.grantExtension(*lee, 999, sam->id(), later, "r"), NotFound);
  EXPECT_THROW(service.grantExtension(*lee, a, outsider->id(), later, "r"), RuleViolation);  // not enrolled
  EXPECT_THROW(service.grantExtension(*lee, a, sam->id(), later, "  "), RuleViolation);      // no reason
  EXPECT_THROW(service.grantExtension(*lee, a, sam->id(), fromIso("2026-11-01T10:00:00Z"), "r"), RuleViolation);  // not later
  EXPECT_THROW(service.grantExtension(*lee, a, sam->id(), fromIso("2026-10-30T10:00:00Z"), "r"), RuleViolation);  // earlier
}

TEST_F(AssignmentFixture, SecondExtensionReplacesFirstButMustBeLater) {
  const int a = newAssignment();
  service.grantExtension(*lee, a, sam->id(), fromIso("2026-11-03T10:00:00Z"), "illness");
  EXPECT_THROW(service.grantExtension(*lee, a, sam->id(), fromIso("2026-11-02T10:00:00Z"), "r"), RuleViolation);
  service.grantExtension(*lee, a, sam->id(), fromIso("2026-11-05T10:00:00Z"), "still ill");
  EXPECT_EQ(toIso(service.effectiveDeadline(a, sam->id())), "2026-11-05T10:00:00Z");
  auto ext = assignmentsRepo.findExtension(a, sam->id());
  EXPECT_EQ(ext->reason, "still ill");
  EXPECT_EQ(ext->grantedBy, lee->id());
}

TEST_F(AssignmentFixture, ExtensionAfterDeadlineMustStillBeInTheFuture) {
  const int a = newAssignment("2026-10-21T10:00:00Z");
  clock.set(fromIso("2026-10-22T10:00:00Z"));  // deadline has passed
  EXPECT_THROW(service.grantExtension(*lee, a, sam->id(), fromIso("2026-10-22T09:00:00Z"), "r"), RuleViolation);
  EXPECT_NO_THROW(service.grantExtension(*lee, a, sam->id(), fromIso("2026-10-24T10:00:00Z"), "r"));
}

// ---- FR8 ----
TEST_F(AssignmentFixture, StudentSeesOwnCoursesAndEffectiveDeadlines) {
  const int a1 = newAssignment("2026-11-01T10:00:00Z");
  service.createAssignment(*lee, course, "Essay 2", fromIso("2026-11-10T10:00:00Z"), 100);
  service.grantExtension(*lee, a1, sam->id(), fromIso("2026-11-03T10:00:00Z"), "illness");

  ASSERT_EQ(service.coursesForStudent(*sam).size(), 1u);
  EXPECT_EQ(service.coursesForStudent(*sam)[0].code, "CS101");
  EXPECT_TRUE(service.coursesForStudent(*outsider).empty());

  auto samView = service.assignmentsForStudent(*sam, course);
  ASSERT_EQ(samView.size(), 2u);
  EXPECT_EQ(samView[0].assignment.title, "Essay 1");   // ordered by deadline
  EXPECT_TRUE(samView[0].hasExtension);
  EXPECT_EQ(toIso(samView[0].effectiveDeadline), "2026-11-03T10:00:00Z");
  EXPECT_FALSE(samView[1].hasExtension);

  auto tomView = service.assignmentsForStudent(*tom, course);
  EXPECT_FALSE(tomView[0].hasExtension);
  EXPECT_EQ(toIso(tomView[0].effectiveDeadline), "2026-11-01T10:00:00Z");
}

TEST_F(AssignmentFixture, StudentsCannotSeeCoursesTheyAreNotEnrolledIn) {
  newAssignment();
  EXPECT_THROW(service.assignmentsForStudent(*outsider, course), AccessDenied);
  EXPECT_THROW(service.coursesForStudent(*lee), AccessDenied);  // a lecturer has no student view
}

// ---- Extension and the late rule work together (BR1, BR2) ----
TEST_F(AssignmentFixture, ExtensionRemovesLatenessForThatStudentOnly) {
  const int a = newAssignment("2026-11-01T10:00:00Z");
  service.grantExtension(*lee, a, sam->id(), fromIso("2026-11-03T10:00:00Z"), "illness");
  const TimePoint submitted = fromIso("2026-11-02T10:00:00Z");  // 1 day after the original deadline
  EXPECT_EQ(daysLate(service.effectiveDeadline(a, sam->id()), submitted), 0);
  EXPECT_EQ(daysLate(service.effectiveDeadline(a, tom->id()), submitted), 1);
}

#include <gtest/gtest.h>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteAssignmentRepository.hpp"
#include "lms/data/SqliteCourseRepository.hpp"
#include "lms/data/SqliteSubmissionRepository.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AdminService.hpp"
#include "lms/services/SubmissionService.hpp"

using namespace lms;
using namespace std::chrono;

class SubmissionFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  LibsodiumHasher hasher;
  SqliteUserRepository users{db};
  SqliteCourseRepository courses{db};
  SqliteAssignmentRepository assignmentsRepo{db};
  SqliteSubmissionRepository submissionsRepo{db};
  AdminService admin{users, courses, hasher};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  AssignmentService assignments{assignmentsRepo, courses, clock};
  SubmissionService service{submissionsRepo, assignmentsRepo, courses, assignments, clock};
  Administrator root{1000, "root", "x", "Root", true};

  const TimePoint deadline = fromIso("2026-11-01T10:00:00Z");
  int dept = 0, course = 0, essay = 0;
  std::unique_ptr<User> lee, otherLecturer, hod, sam, tom, ann, outsider;

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
    ann = make("ann", Role::Student);
    outsider = make("zed", Role::Student);
    admin.assignLecturer(root, course, lee->id());
    for (auto* s : {sam.get(), tom.get(), ann.get()}) admin.enrolStudent(root, s->id(), course);
    essay = assignments.createAssignment(*lee, course, "Essay 1", deadline, 50);
  }

  // Submits at the given time and returns the saved submission.
  Submission submitAt(const User& student, TimePoint when, const std::string& text = "my answer") {
    clock.set(when);
    service.submit(student, essay, text);
    return *submissionsRepo.find(essay, student.id());
  }
};

// ---- FR9: submitting on time ----
TEST_F(SubmissionFixture, OnTimeSubmissionIsRecordedWithTimestamp) {
  auto s = submitAt(*sam, deadline - hours(2));
  EXPECT_EQ(s.state, SubmissionState::Submitted);
  EXPECT_EQ(s.daysLate, 0);
  EXPECT_EQ(toIso(s.submittedAt), "2026-11-01T08:00:00Z");
}

TEST_F(SubmissionFixture, SubmittingExactlyAtTheDeadlineIsOnTime) {
  auto s = submitAt(*sam, deadline);  // boundary: deadline itself
  EXPECT_EQ(s.state, SubmissionState::Submitted);
  EXPECT_EQ(s.daysLate, 0);
}

// ---- FR10, BR3: late work is accepted and the days late are stored ----
TEST_F(SubmissionFixture, LateDaysBoundaryValues) {
  struct Case { seconds after; int days; };
  const Case cases[] = {{seconds(1), 1}, {hours(24), 1}, {hours(24) + seconds(1), 2},
                        {hours(72), 3}, {hours(72) + seconds(1), 4}, {hours(240), 10}};
  for (const auto& c : cases) {
    auto s = submitAt(*sam, deadline + c.after);
    EXPECT_EQ(s.state, SubmissionState::Late) << c.after.count();
    EXPECT_EQ(s.daysLate, c.days) << c.after.count();
    db.exec("DELETE FROM submission_version; DELETE FROM submission;");  // reset for the next case
  }
}

TEST_F(SubmissionFixture, VeryLateWorkIsStillAccepted) {
  EXPECT_NO_THROW(submitAt(*sam, deadline + hours(24 * 30)));
  EXPECT_EQ(submissionsRepo.find(essay, sam->id())->daysLate, 30);
}

// ---- BR2: an extension moves the deadline used for lateness ----
TEST_F(SubmissionFixture, ExtensionIsUsedForLateness) {
  clock.set(fromIso("2026-10-25T08:00:00Z"));
  assignments.grantExtension(*lee, essay, sam->id(), deadline + hours(48), "illness");
  auto samSub = submitAt(*sam, deadline + hours(24));  // 1 day after the original deadline
  auto tomSub = submitAt(*tom, deadline + hours(24));
  EXPECT_EQ(samSub.state, SubmissionState::Submitted);
  EXPECT_EQ(samSub.daysLate, 0);
  EXPECT_EQ(tomSub.state, SubmissionState::Late);
  EXPECT_EQ(tomSub.daysLate, 1);
  auto annSub = submitAt(*ann, deadline + hours(49));  // ann has no extension
  EXPECT_EQ(annSub.daysLate, 3);
}

TEST_F(SubmissionFixture, LatenessCountsFromTheExtendedDeadline) {
  clock.set(fromIso("2026-10-25T08:00:00Z"));
  assignments.grantExtension(*lee, essay, sam->id(), deadline + hours(48), "illness");
  auto s = submitAt(*sam, deadline + hours(48) + seconds(1));
  EXPECT_EQ(s.state, SubmissionState::Late);
  EXPECT_EQ(s.daysLate, 1);
}

// ---- FR11: resubmission keeps every version ----
TEST_F(SubmissionFixture, ResubmissionKeepsAllVersions) {
  EXPECT_EQ(service.submit(*sam, essay, "draft one"), 1);
  clock.advance(hours(1));
  EXPECT_EQ(service.submit(*sam, essay, "draft two"), 2);
  clock.advance(hours(1));
  EXPECT_EQ(service.submit(*sam, essay, "final"), 3);

  auto versions = service.myVersions(*sam, essay);
  ASSERT_EQ(versions.size(), 3u);
  EXPECT_EQ(versions[0].content, "draft one");
  EXPECT_EQ(versions[1].content, "draft two");
  EXPECT_EQ(versions[2].content, "final");
  EXPECT_EQ(toIso(versions[0].submittedAt), "2026-10-20T08:00:00Z");
  EXPECT_EQ(toIso(service.mySubmission(*sam, essay)->submittedAt), "2026-10-20T10:00:00Z");  // latest
}

TEST_F(SubmissionFixture, NoResubmissionAfterTheDeadline) {
  submitAt(*sam, deadline - hours(1));
  clock.set(deadline + seconds(1));
  EXPECT_THROW(service.submit(*sam, essay, "too late"), RuleViolation);
  EXPECT_EQ(service.myVersions(*sam, essay).size(), 1u);  // original kept, still on time
  EXPECT_EQ(service.mySubmission(*sam, essay)->state, SubmissionState::Submitted);
}

TEST_F(SubmissionFixture, LateSubmissionCannotBeReplaced) {
  submitAt(*sam, deadline + hours(1));
  clock.advance(hours(1));
  EXPECT_THROW(service.submit(*sam, essay, "again"), RuleViolation);
}

TEST_F(SubmissionFixture, ExtensionGrantedAfterLateSubmissionReopensResubmission) {
  submitAt(*sam, deadline + hours(1));  // late, 1 day
  assignments.grantExtension(*lee, essay, sam->id(), deadline + hours(48), "illness");
  clock.advance(hours(1));
  EXPECT_EQ(service.submit(*sam, essay, "on time now"), 2);
  auto s = service.mySubmission(*sam, essay);
  EXPECT_EQ(s->state, SubmissionState::Submitted);
  EXPECT_EQ(s->daysLate, 0);
}

TEST_F(SubmissionFixture, NoResubmissionAfterMarking) {
  submitAt(*sam, deadline - hours(5));
  db.exec("UPDATE submission SET state = 'MARKED', raw_mark = 40");  // marking comes in slice 4
  clock.advance(hours(1));
  EXPECT_THROW(service.submit(*sam, essay, "changed"), RuleViolation);
}

// ---- Input and access checks ----
TEST_F(SubmissionFixture, RejectsEmptyOrTooLongContent) {
  EXPECT_THROW(service.submit(*sam, essay, ""), RuleViolation);
  EXPECT_THROW(service.submit(*sam, essay, "   \n "), RuleViolation);
  EXPECT_THROW(service.submit(*sam, essay, std::string(10001, 'a')), RuleViolation);  // boundary: 10001
  EXPECT_EQ(service.submit(*sam, essay, std::string(10000, 'a')), 1);                 // boundary: 10000
}

TEST_F(SubmissionFixture, OnlyEnrolledStudentsMaySubmit) {
  EXPECT_THROW(service.submit(*outsider, essay, "x"), AccessDenied);
  EXPECT_THROW(service.submit(*lee, essay, "x"), AccessDenied);
  EXPECT_THROW(service.submit(*hod, essay, "x"), AccessDenied);
  EXPECT_THROW(service.submit(*sam, 999, "x"), NotFound);
  EXPECT_THROW(service.mySubmission(*outsider, essay), AccessDenied);
}

TEST_F(SubmissionFixture, StudentSeesOnlyOwnSubmission) {
  submitAt(*sam, deadline - hours(1), "sam's work");
  EXPECT_FALSE(service.mySubmission(*tom, essay).has_value());
  EXPECT_TRUE(service.myVersions(*tom, essay).empty());
}

// ---- FR12: lecturer's status list ----
TEST_F(SubmissionFixture, LecturerSeesStatusOfEveryEnrolledStudent) {
  submitAt(*sam, deadline - hours(1), "v1");
  service.submit(*sam, essay, "v2");
  submitAt(*tom, deadline + hours(30));

  auto rows = service.statusForAssignment(*lee, essay);
  ASSERT_EQ(rows.size(), 3u);  // sam, tom, ann (ordered by id)
  EXPECT_EQ(rows[0].studentId, sam->id());
  EXPECT_TRUE(rows[0].submitted);
  EXPECT_EQ(rows[0].versionCount, 2);
  EXPECT_EQ(rows[0].submission->state, SubmissionState::Submitted);
  EXPECT_EQ(rows[1].submission->state, SubmissionState::Late);
  EXPECT_EQ(rows[1].submission->daysLate, 2);
  EXPECT_FALSE(rows[2].submitted);  // ann has not submitted
  EXPECT_EQ(rows[2].versionCount, 0);
}

TEST_F(SubmissionFixture, OnlyTheCoursesLecturerSeesTheStatusList) {
  EXPECT_THROW(service.statusForAssignment(*otherLecturer, essay), AccessDenied);
  EXPECT_THROW(service.statusForAssignment(*sam, essay), AccessDenied);
  EXPECT_THROW(service.statusForAssignment(*hod, essay), AccessDenied);
  EXPECT_THROW(service.statusForAssignment(*lee, 999), NotFound);
}

// ---- Database rules ----
TEST_F(SubmissionFixture, VersionAndSubmissionAreSavedTogether) {
  submitAt(*sam, deadline - hours(1));
  db.exec("CREATE TRIGGER block_v BEFORE INSERT ON submission_version "
          "BEGIN SELECT RAISE(ABORT, 'simulated failure'); END;");
  clock.advance(minutes(5));
  EXPECT_THROW(service.submit(*sam, essay, "second"), RuleViolation);
  // the rollback left the first submission untouched
  EXPECT_EQ(toIso(service.mySubmission(*sam, essay)->submittedAt), "2026-11-01T09:00:00Z");
}

TEST(SubmissionState, TextRoundTrip) {
  for (auto s : {SubmissionState::Submitted, SubmissionState::Late, SubmissionState::Marked,
                 SubmissionState::Moderated, SubmissionState::Released})
    EXPECT_EQ(stateFromString(stateToString(s)), s);
  EXPECT_THROW(stateFromString("DONE"), RuleViolation);
}

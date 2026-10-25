#include <gtest/gtest.h>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteAssignmentRepository.hpp"
#include "lms/data/SqliteCourseRepository.hpp"
#include "lms/data/SqliteSubmissionRepository.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AdminService.hpp"
#include "lms/services/MarkingService.hpp"
#include "lms/services/SubmissionService.hpp"

using namespace lms;
using namespace std::chrono;

class MarkingFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  LibsodiumHasher hasher;
  SqliteUserRepository users{db};
  SqliteCourseRepository courses{db};
  SqliteAssignmentRepository assignmentsRepo{db};
  SqliteSubmissionRepository submissionsRepo{db};
  AdminService admin{users, courses, hasher};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  StandardPenaltyStrategy penalty;
  AssignmentService assignments{assignmentsRepo, courses, clock};
  SubmissionService submissions{submissionsRepo, assignmentsRepo, courses, assignments, clock};
  MarkingService service{submissionsRepo, assignmentsRepo, courses, penalty, clock};
  Administrator root{1000, "root", "x", "Root", true};

  const TimePoint deadline = fromIso("2026-11-01T10:00:00Z");
  int essay = 0;
  std::unique_ptr<User> lee, otherLecturer, hod, otherHod, sam, tom;

  std::unique_ptr<User> make(const std::string& name, Role role, std::optional<int> d = std::nullopt) {
    return users.findById(admin.createUser(root, name, "password1", name, role, d));
  }

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    const int science = admin.createDepartment(root, "Science");
    const int arts = admin.createDepartment(root, "Arts");
    const int course = admin.createCourse(root, "CS101", "Intro", science);
    lee = make("lee", Role::Lecturer, science);
    otherLecturer = make("olga", Role::Lecturer, science);
    hod = make("hal", Role::HeadOfDepartment, science);
    otherHod = make("ari", Role::HeadOfDepartment, arts);
    sam = make("sam", Role::Student);
    tom = make("tom", Role::Student);
    admin.assignLecturer(root, course, lee->id());
    admin.enrolStudent(root, sam->id(), course);
    admin.enrolStudent(root, tom->id(), course);
    essay = assignments.createAssignment(*lee, course, "Essay 1", deadline, 50);
  }

  void submitAt(const User& student, TimePoint when) {
    clock.set(when);
    submissions.submit(student, essay, "my answer");
  }
};

// ---- FR13: marking and feedback ----
TEST_F(MarkingFixture, LecturerMarksSubmissionWithFeedback) {
  submitAt(*sam, deadline - hours(1));
  service.markSubmission(*lee, essay, sam->id(), 40, "Good structure");
  auto s = submissionsRepo.find(essay, sam->id());
  EXPECT_EQ(s->state, SubmissionState::Marked);
  EXPECT_EQ(*s->rawMark, 40);
  EXPECT_EQ(s->feedback, "Good structure");
  EXPECT_EQ(*s->markedBy, lee->id());
}

TEST_F(MarkingFixture, MarkBoundaryValues) {
  submitAt(*sam, deadline - hours(1));
  EXPECT_THROW(service.markSubmission(*lee, essay, sam->id(), -1, "f"), RuleViolation);  // below 0
  EXPECT_THROW(service.markSubmission(*lee, essay, sam->id(), 51, "f"), RuleViolation);  // above max
  EXPECT_NO_THROW(service.markSubmission(*lee, essay, sam->id(), 50, "f"));             // max
  submitAt(*tom, deadline - hours(1));
  EXPECT_NO_THROW(service.markSubmission(*lee, essay, tom->id(), 0, "f"));              // 0
}

TEST_F(MarkingFixture, FeedbackIsRequiredAndLimited) {
  submitAt(*sam, deadline - hours(1));
  EXPECT_THROW(service.markSubmission(*lee, essay, sam->id(), 30, "  "), RuleViolation);
  EXPECT_THROW(service.markSubmission(*lee, essay, sam->id(), 30, std::string(2001, 'a')), RuleViolation);
  EXPECT_NO_THROW(service.markSubmission(*lee, essay, sam->id(), 30, std::string(2000, 'a')));
}

TEST_F(MarkingFixture, CannotMarkTwiceOrMarkMissingWork) {
  submitAt(*sam, deadline - hours(1));
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  EXPECT_THROW(service.markSubmission(*lee, essay, sam->id(), 45, "again"), RuleViolation);
  EXPECT_THROW(service.markSubmission(*lee, essay, tom->id(), 40, "ok"), NotFound);  // tom never submitted
  EXPECT_THROW(service.markSubmission(*lee, 999, sam->id(), 40, "ok"), NotFound);
}

TEST_F(MarkingFixture, OnlyTheCoursesLecturerMayMark) {
  submitAt(*sam, deadline - hours(1));
  EXPECT_THROW(service.markSubmission(*otherLecturer, essay, sam->id(), 40, "f"), AccessDenied);
  EXPECT_THROW(service.markSubmission(*hod, essay, sam->id(), 40, "f"), AccessDenied);
  EXPECT_THROW(service.markSubmission(*sam, essay, sam->id(), 50, "f"), AccessDenied);
}

// ---- BR1: the late penalty is applied to the raw mark ----
TEST_F(MarkingFixture, LatePenaltyTable) {
  struct Case { seconds after; int raw, penalty, final; };
  const Case cases[] = {
      {-hours(1), 40, 0, 40},                // on time
      {hours(1), 40, 5, 35},                 // 1 day: 10% of 50
      {hours(25), 40, 10, 30},               // 2 days: 20%
      {hours(72), 40, 15, 25},               // 3 days: 30%
      {hours(72) + seconds(1), 40, 40, 0},   // more than 3 days: zero
      {hours(72), 10, 10, 0},                // never below zero
  };
  for (const auto& c : cases) {
    submitAt(*sam, deadline + c.after);
    service.markSubmission(*lee, essay, sam->id(), c.raw, "f");
    auto d = service.markDetails(*lee, essay, sam->id());
    EXPECT_EQ(d.rawMark, c.raw) << c.after.count();
    EXPECT_EQ(d.penalty, c.penalty) << c.after.count();
    EXPECT_EQ(d.finalMark, c.final) << c.after.count();
    EXPECT_EQ(d.maxMark, 50);
    db.exec("DROP TRIGGER audit_no_delete");  // test-only reset between cases
    db.exec("DELETE FROM audit_log; DELETE FROM submission_version; DELETE FROM submission;");
    db.exec("CREATE TRIGGER audit_no_delete BEFORE DELETE ON audit_log "
            "BEGIN SELECT RAISE(ABORT, 'audit_log is append-only'); END;");
  }
}

TEST_F(MarkingFixture, ExtensionMeansNoPenalty) {
  assignments.grantExtension(*lee, essay, sam->id(), deadline + hours(48), "illness");
  submitAt(*sam, deadline + hours(30));
  service.markSubmission(*lee, essay, sam->id(), 40, "f");
  EXPECT_EQ(service.markDetails(*lee, essay, sam->id()).finalMark, 40);
}

// ---- FR14: changing a mark needs a reason ----
TEST_F(MarkingFixture, ChangeMarkNeedsReasonAndUpdatesMark) {
  submitAt(*sam, deadline - hours(1));
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  EXPECT_THROW(service.changeMark(*lee, essay, sam->id(), 45, "better", "  "), RuleViolation);
  service.changeMark(*lee, essay, sam->id(), 45, "better", "missed a page when marking");
  auto d = service.markDetails(*lee, essay, sam->id());
  EXPECT_EQ(d.rawMark, 45);
  EXPECT_EQ(d.feedback, "better");
}

TEST_F(MarkingFixture, ChangeMarkRules) {
  submitAt(*sam, deadline - hours(1));
  EXPECT_THROW(service.changeMark(*lee, essay, sam->id(), 45, "f", "r"), RuleViolation);  // not marked yet
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  EXPECT_THROW(service.changeMark(*lee, essay, sam->id(), 51, "f", "r"), RuleViolation);  // above max
  EXPECT_THROW(service.changeMark(*otherLecturer, essay, sam->id(), 45, "f", "r"), AccessDenied);
  db.exec("UPDATE submission SET state = 'MODERATED'");  // moderation comes in slice 5
  EXPECT_THROW(service.changeMark(*lee, essay, sam->id(), 45, "f", "r"), RuleViolation);
}

TEST_F(MarkingFixture, StudentCannotResubmitAfterMarking) {
  submitAt(*sam, deadline - hours(5));
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  clock.advance(hours(1));
  EXPECT_THROW(submissions.submit(*sam, essay, "new version"), RuleViolation);
}

// ---- FR15, NFR11: every mark change is audited ----
TEST_F(MarkingFixture, AuditTrailRecordsWhoWhenOldNewAndWhy) {
  submitAt(*sam, deadline - hours(1));
  clock.set(fromIso("2026-11-02T09:00:00Z"));
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  clock.set(fromIso("2026-11-03T14:30:00Z"));
  service.changeMark(*lee, essay, sam->id(), 44, "ok", "added marks for references");

  auto log = service.auditTrail(*lee, essay, sam->id());
  ASSERT_EQ(log.size(), 2u);
  EXPECT_EQ(log[0].changedBy, lee->id());
  EXPECT_EQ(toIso(log[0].changedAt), "2026-11-02T09:00:00Z");
  EXPECT_FALSE(log[0].oldValue.has_value());
  EXPECT_EQ(*log[0].newValue, 40);
  EXPECT_EQ(log[0].reason, "initial marking");
  EXPECT_EQ(toIso(log[1].changedAt), "2026-11-03T14:30:00Z");
  EXPECT_EQ(*log[1].oldValue, 40);
  EXPECT_EQ(*log[1].newValue, 44);
  EXPECT_EQ(log[1].reason, "added marks for references");
}

TEST_F(MarkingFixture, AuditTrailVisibility) {
  submitAt(*sam, deadline - hours(1));
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  EXPECT_EQ(service.auditTrail(*hod, essay, sam->id()).size(), 1u);  // HoD of Science
  EXPECT_THROW(service.auditTrail(*otherHod, essay, sam->id()), AccessDenied);  // HoD of Arts
  EXPECT_THROW(service.auditTrail(*otherLecturer, essay, sam->id()), AccessDenied);
  EXPECT_THROW(service.auditTrail(*sam, essay, sam->id()), AccessDenied);
}

TEST_F(MarkingFixture, MarkIsNotSavedIfAuditFails) {
  submitAt(*sam, deadline - hours(1));
  db.exec("CREATE TRIGGER block_audit BEFORE INSERT ON audit_log "
          "BEGIN SELECT RAISE(ABORT, 'simulated failure'); END;");
  EXPECT_THROW(service.markSubmission(*lee, essay, sam->id(), 40, "ok"), RuleViolation);
  auto s = submissionsRepo.find(essay, sam->id());
  EXPECT_FALSE(s->rawMark.has_value());  // rolled back together with the audit entry
  EXPECT_EQ(s->state, SubmissionState::Submitted);
}

TEST_F(MarkingFixture, AuditLogCannotBeEdited) {
  submitAt(*sam, deadline - hours(1));
  service.markSubmission(*lee, essay, sam->id(), 40, "ok");
  EXPECT_THROW(db.exec("UPDATE audit_log SET new_value = 50"), DatabaseError);
  EXPECT_THROW(db.exec("DELETE FROM audit_log"), DatabaseError);
  EXPECT_EQ(*service.auditTrail(*lee, essay, sam->id())[0].newValue, 40);
}

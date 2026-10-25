#include <gtest/gtest.h>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteAssignmentRepository.hpp"
#include "lms/data/SqliteAuditRepository.hpp"
#include "lms/data/SqliteCourseRepository.hpp"
#include "lms/data/SqliteSubmissionRepository.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AdminService.hpp"
#include "lms/services/AuditObserver.hpp"
#include "lms/services/ModerationService.hpp"
#include "lms/services/StatusObserver.hpp"
#include "lms/services/SubmissionService.hpp"

using namespace lms;
using namespace std::chrono;

class ModerationFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  LibsodiumHasher hasher;
  SqliteUserRepository users{db};
  SqliteCourseRepository courses{db};
  SqliteAssignmentRepository assignmentsRepo{db};
  SqliteSubmissionRepository submissionsRepo{db};
  SqliteAuditRepository auditRepo{db};
  AuditObserver auditObserver{auditRepo};
  StatusObserver statusObserver{submissionsRepo};
  AdminService admin{users, courses, hasher};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  StandardPenaltyStrategy penalty;
  AssignmentService assignments{assignmentsRepo, courses, clock};
  SubmissionService submissions{submissionsRepo, assignmentsRepo, courses, assignments, clock};
  MarkingService marking{submissionsRepo, assignmentsRepo, courses, auditRepo, db, penalty, clock};
  ModerationService service{submissionsRepo, assignmentsRepo, courses, db, penalty, clock};
  Administrator root{1000, "root", "x", "Root", true};

  const TimePoint deadline = fromIso("2026-11-01T10:00:00Z");
  int essay = 0, artEssay = 0;
  std::unique_ptr<User> lee, artLecturer, hod, artHod, sam, tom, ann;

  std::unique_ptr<User> make(const std::string& name, Role role, std::optional<int> d = std::nullopt) {
    return users.findById(admin.createUser(root, name, "password1", name, role, d));
  }

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    for (ISubmissionObserver* o : {static_cast<ISubmissionObserver*>(&auditObserver),
                                   static_cast<ISubmissionObserver*>(&statusObserver)}) {
      marking.attach(*o);
      service.attach(*o);
    }
    const int science = admin.createDepartment(root, "Science");
    const int arts = admin.createDepartment(root, "Arts");
    const int cs = admin.createCourse(root, "CS101", "Intro", science);
    const int art = admin.createCourse(root, "ART101", "Drawing", arts);
    lee = make("lee", Role::Lecturer, science);
    artLecturer = make("ada", Role::Lecturer, arts);
    hod = make("hal", Role::HeadOfDepartment, science);
    artHod = make("ari", Role::HeadOfDepartment, arts);
    sam = make("sam", Role::Student);
    tom = make("tom", Role::Student);
    ann = make("ann", Role::Student);
    admin.assignLecturer(root, cs, lee->id());
    admin.assignLecturer(root, art, artLecturer->id());
    for (auto* s : {sam.get(), tom.get(), ann.get()}) admin.enrolStudent(root, s->id(), cs);
    admin.enrolStudent(root, sam->id(), art);
    essay = assignments.createAssignment(*lee, cs, "Essay 1", deadline, 50);
    artEssay = assignments.createAssignment(*artLecturer, art, "Sketch", deadline, 20);
  }

  // Submits on time (or late) and marks it; returns the submission id.
  int submitAndMark(const User& student, int raw, seconds lateBy = -hours(1)) {
    clock.set(deadline + lateBy);
    submissions.submit(student, essay, "answer");
    marking.markSubmission(*lee, essay, student.id(), raw, "feedback for " + student.username());
    return submissionsRepo.find(essay, student.id())->id;
  }
};

// ---- FR15, FR16: HoD sees and approves marks in their own department ----
TEST_F(ModerationFixture, HodSeesMarkedWorkFromOwnDepartmentOnly) {
  submitAndMark(*sam, 40);
  submitAndMark(*tom, 30, hours(1));  // 1 day late
  clock.set(deadline - hours(1));
  submissions.submit(*ann, essay, "not marked yet");

  auto items = service.listForModeration(*hod);
  ASSERT_EQ(items.size(), 2u);  // ann is not marked yet
  EXPECT_EQ(items[0].submission.studentId, sam->id());
  EXPECT_EQ(items[0].details.finalMark, 40);
  EXPECT_EQ(items[1].details.finalMark, 25);  // 30 minus 10% of 50
  EXPECT_EQ(items[1].assignment.title, "Essay 1");
  EXPECT_TRUE(service.listForModeration(*artHod).empty());
}

TEST_F(ModerationFixture, ApproveKeepsMarkAndIsAudited) {
  const int id = submitAndMark(*sam, 40);
  clock.set(fromIso("2026-11-05T09:00:00Z"));
  service.approve(*hod, id);
  auto s = submissionsRepo.findById(id);
  EXPECT_EQ(s->state, SubmissionState::Moderated);
  EXPECT_EQ(*s->rawMark, 40);
  EXPECT_EQ(*s->moderatedBy, hod->id());
  auto log = marking.auditTrail(*hod, essay, sam->id());
  ASSERT_EQ(log.size(), 2u);
  EXPECT_EQ(log[1].changedBy, hod->id());
  EXPECT_EQ(*log[1].oldValue, 40);
  EXPECT_EQ(*log[1].newValue, 40);
  EXPECT_EQ(log[1].reason, "approved in moderation");
  EXPECT_EQ(toIso(log[1].changedAt), "2026-11-05T09:00:00Z");
}

// ---- FR16: adjusting needs a reason and is audited ----
TEST_F(ModerationFixture, AdjustChangesMarkWithReason) {
  const int id = submitAndMark(*sam, 40);
  EXPECT_THROW(service.adjustMark(*hod, id, 35, " "), RuleViolation);
  service.adjustMark(*hod, id, 35, "too generous for section 2");
  auto s = submissionsRepo.findById(id);
  EXPECT_EQ(s->state, SubmissionState::Moderated);
  EXPECT_EQ(*s->rawMark, 35);
  auto log = marking.auditTrail(*hod, essay, sam->id());
  EXPECT_EQ(*log.back().oldValue, 40);
  EXPECT_EQ(*log.back().newValue, 35);
  EXPECT_EQ(log.back().reason, "too generous for section 2");
}

TEST_F(ModerationFixture, AdjustBoundaryValues) {
  const int id = submitAndMark(*sam, 40);
  EXPECT_THROW(service.adjustMark(*hod, id, -1, "r"), RuleViolation);
  EXPECT_THROW(service.adjustMark(*hod, id, 51, "r"), RuleViolation);
  EXPECT_NO_THROW(service.adjustMark(*hod, id, 50, "r"));
  const int id2 = submitAndMark(*tom, 40);
  EXPECT_NO_THROW(service.adjustMark(*hod, id2, 0, "r"));
}

TEST_F(ModerationFixture, OnlyMarkedWorkCanBeModeratedAndOnlyOnce) {
  clock.set(deadline - hours(1));
  submissions.submit(*sam, essay, "answer");
  const int notMarked = submissionsRepo.find(essay, sam->id())->id;
  EXPECT_THROW(service.approve(*hod, notMarked), RuleViolation);
  const int id = submitAndMark(*tom, 40);
  service.approve(*hod, id);
  EXPECT_THROW(service.approve(*hod, id), RuleViolation);       // already moderated
  EXPECT_THROW(service.adjustMark(*hod, id, 30, "r"), RuleViolation);
  EXPECT_THROW(service.approve(*hod, 999), NotFound);
}

TEST_F(ModerationFixture, OnlyTheRightHodMayModerate) {
  const int id = submitAndMark(*sam, 40);
  EXPECT_THROW(service.approve(*artHod, id), AccessDenied);  // other department
  EXPECT_THROW(service.approve(*lee, id), AccessDenied);     // lecturer
  EXPECT_THROW(service.adjustMark(*sam, id, 50, "r"), AccessDenied);
  EXPECT_THROW(service.listForModeration(*lee), AccessDenied);
  EXPECT_THROW(service.release(*artHod, essay), AccessDenied);
}

TEST_F(ModerationFixture, LecturerCannotChangeMarkAfterModeration) {
  const int id = submitAndMark(*sam, 40);
  service.approve(*hod, id);
  EXPECT_THROW(marking.changeMark(*lee, essay, sam->id(), 45, "f", "r"), RuleViolation);
}

// ---- FR17: releasing approved marks ----
TEST_F(ModerationFixture, ReleaseOnlyReleasesModeratedWork) {
  const int a = submitAndMark(*sam, 40);
  submitAndMark(*tom, 30);  // marked but not moderated
  service.approve(*hod, a);
  clock.set(fromIso("2026-11-06T12:00:00Z"));
  EXPECT_THROW(service.release(*hod, essay), RuleViolation);  // tom is not moderated (UC09 E3)
  EXPECT_EQ(submissionsRepo.find(essay, sam->id())->state, SubmissionState::Moderated);  // nothing released
  EXPECT_EQ(service.release(*hod, essay, true), 1);  // partial release (UC09 A3)
  auto samSub = submissionsRepo.find(essay, sam->id());
  EXPECT_EQ(samSub->state, SubmissionState::Released);
  EXPECT_EQ(toIso(*samSub->releasedAt), "2026-11-06T12:00:00Z");
  EXPECT_EQ(submissionsRepo.find(essay, tom->id())->state, SubmissionState::Marked);
  EXPECT_EQ(service.release(*hod, essay, true), 0);  // nothing new to release
}

// ---- FR18, BR4: students see marks only after release ----
TEST_F(ModerationFixture, StudentSeesNothingBeforeRelease) {
  const int id = submitAndMark(*sam, 40);
  auto before = service.resultFor(*sam, essay);
  EXPECT_TRUE(before.submitted);
  EXPECT_FALSE(before.released);
  EXPECT_FALSE(before.details.has_value());  // marked, but hidden
  service.approve(*hod, id);
  EXPECT_FALSE(service.resultFor(*sam, essay).details.has_value());  // approved, still hidden
}

TEST_F(ModerationFixture, StudentSeesFinalMarkAndFeedbackAfterRelease) {
  const int id = submitAndMark(*sam, 40, hours(25));  // 2 days late
  service.adjustMark(*hod, id, 44, "missed credit for diagrams");
  service.release(*hod, essay);
  auto r = service.resultFor(*sam, essay);
  ASSERT_TRUE(r.details.has_value());
  EXPECT_TRUE(r.released);
  EXPECT_EQ(r.details->rawMark, 44);
  EXPECT_EQ(r.details->daysLate, 2);
  EXPECT_EQ(r.details->penalty, 10);
  EXPECT_EQ(r.details->finalMark, 34);
  EXPECT_EQ(r.details->feedback, "feedback for sam");
}

TEST_F(ModerationFixture, StudentResultAccessRules) {
  EXPECT_FALSE(service.resultFor(*sam, essay).submitted);  // nothing submitted yet
  EXPECT_THROW(service.resultFor(*lee, essay), AccessDenied);
  EXPECT_THROW(service.resultFor(*tom, artEssay), AccessDenied);  // tom is not in ART101
  EXPECT_THROW(service.resultFor(*sam, 999), NotFound);
}

TEST_F(ModerationFixture, ModerationIsNotSavedIfAuditFails) {
  const int id = submitAndMark(*sam, 40);
  db.exec("CREATE TRIGGER block_audit BEFORE INSERT ON audit_log "
          "BEGIN SELECT RAISE(ABORT, 'simulated failure'); END;");
  EXPECT_THROW(service.adjustMark(*hod, id, 30, "r"), DatabaseError);
  auto s = submissionsRepo.findById(id);
  EXPECT_EQ(s->state, SubmissionState::Marked);  // rolled back
  EXPECT_EQ(*s->rawMark, 40);
}

// ---- Whole life cycle: submitted -> marked -> moderated -> released ----
TEST_F(ModerationFixture, FullLifeCycle) {
  clock.set(deadline - hours(2));
  submissions.submit(*sam, essay, "v1");
  EXPECT_EQ(submissionsRepo.find(essay, sam->id())->state, SubmissionState::Submitted);
  marking.markSubmission(*lee, essay, sam->id(), 42, "well argued");
  EXPECT_EQ(submissionsRepo.find(essay, sam->id())->state, SubmissionState::Marked);
  service.approve(*hod, submissionsRepo.find(essay, sam->id())->id);
  EXPECT_EQ(submissionsRepo.find(essay, sam->id())->state, SubmissionState::Moderated);
  service.release(*hod, essay);
  EXPECT_EQ(submissionsRepo.find(essay, sam->id())->state, SubmissionState::Released);
  EXPECT_EQ(service.resultFor(*sam, essay).details->finalMark, 42);
}

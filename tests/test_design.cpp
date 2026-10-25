// Tests for the design elements added to match the Phase 3 class diagram:
// the submission state machine, the Observer pattern, InputValidator, bulk approval
// and editing departments.
#include <gtest/gtest.h>

#include "lms/app/LmsServices.hpp"
#include "lms/services/InputValidator.hpp"

using namespace lms;
using namespace std::chrono;
using S = SubmissionState;

// ---- FR14: life cycle, every pair of states ----
TEST(StateMachine, OnlyAllowedTransitions) {
  const S all[] = {S::Submitted, S::Late, S::Marked, S::Moderated, S::Released};
  auto allowed = [](S from, S to) {
    if (from == S::Submitted || from == S::Late) return to == S::Submitted || to == S::Late || to == S::Marked;
    if (from == S::Marked) return to == S::Moderated;
    if (from == S::Moderated) return to == S::Released;
    return false;
  };
  for (S from : all)
    for (S to : all) {
      Submission s;
      s.state = from;
      if (allowed(from, to)) {
        EXPECT_NO_THROW(s.transitionTo(to)) << stateToString(from) << "->" << stateToString(to);
        EXPECT_EQ(s.state, to);
      } else {
        EXPECT_THROW(s.transitionTo(to), RuleViolation) << stateToString(from) << "->" << stateToString(to);
        EXPECT_EQ(s.state, from);  // unchanged after a refused move
      }
    }
}

TEST(StateMachine, FinalMarkUsesPenaltyStrategy) {
  StandardPenaltyStrategy penalty;
  Submission s;
  EXPECT_EQ(s.finalMark(50, penalty), 0);  // not marked yet
  s.rawMark = 40;
  for (auto [days, expected] : {std::pair{0, 40}, {1, 35}, {2, 30}, {3, 25}, {4, 0}}) {
    s.daysLate = days;
    EXPECT_EQ(s.finalMark(50, penalty), expected) << days;
  }
}

// ---- InputValidator ----
TEST(Validator, ParsesNumbersAndDates) {
  EXPECT_EQ(InputValidator::parseInt("42"), 42);
  EXPECT_EQ(InputValidator::parseInt("-3"), -3);
  for (const char* bad : {"", "abc", "4 2", "12x", "99999999999"})
    EXPECT_FALSE(InputValidator::parseInt(bad).has_value()) << bad;
  EXPECT_EQ(toIso(*InputValidator::parseDateTime("2026-11-01 10:00")), "2026-11-01T10:00:00Z");
  for (const char* bad : {"2026-11-01", "1 Nov 2026", "2026-02-30 10:00", "2026-11-01T10:00", "2026-11-01 25:00"})
    EXPECT_FALSE(InputValidator::parseDateTime(bad).has_value()) << bad;
}

TEST(Validator, RequireTextAndRange) {
  EXPECT_THROW(InputValidator::requireText(" \t", "name", 10), RuleViolation);
  EXPECT_THROW(InputValidator::requireText("abcdefghijk", "name", 10), RuleViolation);  // 11 chars
  EXPECT_NO_THROW(InputValidator::requireText("abcdefghij", "name", 10));               // 10 chars
  EXPECT_THROW(InputValidator::requireRange(-1, 0, 50, "mark"), RuleViolation);
  EXPECT_THROW(InputValidator::requireRange(51, 0, 50, "mark"), RuleViolation);
  EXPECT_NO_THROW(InputValidator::requireRange(0, 0, 50, "mark"));
  EXPECT_NO_THROW(InputValidator::requireRange(50, 0, 50, "mark"));
}

// ---- Observer pattern and the other additions, through the real wiring ----
namespace {
// A test observer that only counts what it is told.
struct CountingObserver : ISubmissionObserver {
  int marks = 0, states = 0;
  void onMarkChanged(const AuditEntry&) override { ++marks; }
  void onStateChanged(int, SubmissionState, SubmissionState) override { ++states; }
};
}  // namespace

class DesignFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  LmsServices s{db, clock};
  Administrator root{1000, "root", "x", "Root", true};
  std::unique_ptr<User> lee, hod, sam, tom, ann;
  int dept = 0, essay = 0;

  std::unique_ptr<User> make(const std::string& name, Role role, std::optional<int> d = std::nullopt) {
    return s.users.findById(s.admin.createUser(root, name, "password1", name, role, d));
  }

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    dept = s.admin.createDepartment(root, "Science");
    const int course = s.admin.createCourse(root, "CS101", "Intro", dept);
    lee = make("lee", Role::Lecturer, dept);
    hod = make("hal", Role::HeadOfDepartment, dept);
    sam = make("sam", Role::Student);
    tom = make("tom", Role::Student);
    ann = make("ann", Role::Student);
    s.admin.assignLecturer(root, course, lee->id());
    for (auto* u : {sam.get(), tom.get(), ann.get()}) s.admin.enrolStudent(root, u->id(), course);
    essay = s.assignments.createAssignment(*lee, course, "Essay 1", fromIso("2026-11-01T10:00:00Z"), 50);
  }

  int submitAndMark(const User& student, int raw) {
    s.submissions.submit(student, essay, "answer");
    s.marking.markSubmission(*lee, essay, student.id(), raw, "ok");
    return s.submissionsRepo.find(essay, student.id())->id;
  }
};

TEST_F(DesignFixture, ServicesNotifyEveryAttachedObserver) {
  CountingObserver counter;
  s.marking.attach(counter);
  s.moderation.attach(counter);
  const int id = submitAndMark(*sam, 40);              // 1 mark change, 1 state change
  s.marking.changeMark(*lee, essay, sam->id(), 42, "ok", "recount");  // 1 mark change
  s.moderation.approve(*hod, id);                     // 1 mark change, 1 state change
  s.moderation.release(*hod, essay);                  // 1 state change
  EXPECT_EQ(counter.marks, 3);
  EXPECT_EQ(counter.states, 3);
}

TEST_F(DesignFixture, AuditObserverWritesExactlyOneEntryPerChange) {
  const int id = submitAndMark(*sam, 40);
  s.marking.changeMark(*lee, essay, sam->id(), 42, "ok", "recount");
  s.moderation.adjustMark(*hod, id, 38, "too generous");
  EXPECT_EQ(s.auditRepo.forSubmission(id).size(), 3u);  // NFR11: one entry per mark change
}

TEST_F(DesignFixture, StatusObserverSavesTheNewState) {
  const int id = submitAndMark(*sam, 40);
  EXPECT_EQ(s.submissionsRepo.findById(id)->state, S::Marked);
  s.moderation.approve(*hod, id);
  EXPECT_EQ(s.submissionsRepo.findById(id)->state, S::Moderated);
}

TEST_F(DesignFixture, FailingObserverRollsBackTheWholeChange) {
  struct Failing : ISubmissionObserver {
    void onMarkChanged(const AuditEntry&) override {}
    void onStateChanged(int, SubmissionState, SubmissionState) override { throw DatabaseError("disk full"); }
  } failing;
  s.submissions.submit(*sam, essay, "answer");
  s.marking.attach(failing);
  EXPECT_THROW(s.marking.markSubmission(*lee, essay, sam->id(), 40, "ok"), DatabaseError);
  const auto sub = s.submissionsRepo.find(essay, sam->id());
  EXPECT_FALSE(sub->rawMark.has_value());               // mark not saved
  EXPECT_TRUE(s.auditRepo.forSubmission(sub->id).empty());  // audit entry not saved either
}

// ---- UC09 A2: bulk approval, one audit entry each ----
TEST_F(DesignFixture, ApproveAllApprovesOnlyMarkedWork) {
  const int a = submitAndMark(*sam, 40);
  const int b = submitAndMark(*tom, 30);
  s.submissions.submit(*ann, essay, "not marked");
  EXPECT_EQ(s.moderation.approveAll(*hod, essay), 2);
  EXPECT_EQ(s.submissionsRepo.findById(a)->state, S::Moderated);
  EXPECT_EQ(s.submissionsRepo.findById(b)->state, S::Moderated);
  EXPECT_EQ(s.submissionsRepo.find(essay, ann->id())->state, S::Submitted);
  EXPECT_EQ(s.auditRepo.forSubmission(a).back().reason, "approved in moderation (bulk)");
  EXPECT_EQ(s.moderation.approveAll(*hod, essay), 0);  // nothing left
  EXPECT_THROW(s.moderation.approveAll(*lee, essay), AccessDenied);
}

// ---- UC09 E3: release refused while work is pending ----
TEST_F(DesignFixture, ReleaseNamesThePendingSubmissions) {
  const int a = submitAndMark(*sam, 40);
  s.submissions.submit(*tom, essay, "not marked");
  s.moderation.approve(*hod, a);
  try {
    s.moderation.release(*hod, essay);
    FAIL() << "release should have been refused";
  } catch (const RuleViolation& e) {
    EXPECT_NE(std::string(e.what()).find(std::to_string(s.submissionsRepo.find(essay, tom->id())->id)),
              std::string::npos);
  }
}

// ---- FR15: lecturer and extension shown to the HoD ----
TEST_F(DesignFixture, ModerationListShowsLecturerAndExtension) {
  s.assignments.grantExtension(*lee, essay, sam->id(), fromIso("2026-11-03T10:00:00Z"), "illness");
  submitAndMark(*sam, 40);
  submitAndMark(*tom, 40);
  const auto items = s.moderation.listForModeration(*hod);
  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(*items[0].lecturerId, lee->id());
  ASSERT_TRUE(items[0].extension.has_value());
  EXPECT_EQ(toIso(items[0].extension->newDeadline), "2026-11-03T10:00:00Z");
  EXPECT_FALSE(items[1].extension.has_value());
}

// ---- FR4: editing departments ----
TEST_F(DesignFixture, AdminRenamesDepartment) {
  s.admin.renameDepartment(root, dept, "Natural Sciences");
  EXPECT_EQ(s.courses.findDepartment(dept)->name, "Natural Sciences");
  s.admin.createDepartment(root, "Arts");
  EXPECT_THROW(s.admin.renameDepartment(root, dept, "Arts"), RuleViolation);  // name taken
  EXPECT_THROW(s.admin.renameDepartment(root, dept, "  "), RuleViolation);
  EXPECT_THROW(s.admin.renameDepartment(root, 999, "X"), NotFound);
  EXPECT_THROW(s.admin.renameDepartment(*lee, dept, "X"), AccessDenied);
}

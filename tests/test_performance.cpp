// NFR1-NFR3: capacity and response times with a full-size school.
// 800 students, 50 lecturers, 30 courses and 25,600 submissions are loaded with
// bulk SQL (test set-up only), then the real services are timed.
#include <gtest/gtest.h>

#include <chrono>

#include "lms/app/LmsServices.hpp"

using namespace lms;

namespace {
double secondsSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

int countRows(DatabaseConnection& db, const std::string& table, const std::string& where = "") {
  auto st = db.prepare("SELECT COUNT(*) FROM " + table + (where.empty() ? "" : " WHERE " + where));
  st.step();
  return st.columnInt(0);
}
}  // namespace

class PerformanceFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  FixedClock clock{fromIso("2026-10-20T08:00:00Z")};
  LmsServices s{db, clock};

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    const std::string hash = s.hasher.hash("password1");  // one real Argon2id hash, reused
    db.runInTransaction([&] {
      db.exec("INSERT INTO department (name) VALUES ('Science')");
      db.exec("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 50) "
              "INSERT INTO app_user (username, password_hash, full_name, role, department_id) "
              "SELECT 'lect' || i, '" + hash + "', 'Lecturer ' || i, 'LECTURER', 1 FROM n");
      db.exec("INSERT INTO app_user (username, password_hash, full_name, role, department_id) "
              "VALUES ('hod', '" + hash + "', 'Head', 'HOD', 1)");
      db.exec("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 800) "
              "INSERT INTO app_user (username, password_hash, full_name, role) "
              "SELECT 'student' || i, '" + hash + "', 'Student ' || i, 'STUDENT' FROM n");
      db.exec("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 30) "
              "INSERT INTO course (code, name, department_id, lecturer_id) "
              "SELECT 'C' || i, 'Course ' || i, 1, "
              "(SELECT user_id FROM app_user WHERE username = 'lect' || (((i - 1) % 50) + 1)) FROM n");
      db.exec("INSERT INTO enrolment (student_id, course_id) "
              "SELECT u.user_id, c.course_id FROM app_user u CROSS JOIN course c WHERE u.role = 'STUDENT'");
      db.exec("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 32) "
              "INSERT INTO assignment (course_id, title, deadline, max_mark) "
              "SELECT ((i - 1) % 30) + 1, 'Assignment ' || i, '2026-11-01T10:00:00Z', 50 FROM n");
      // assignment 1 is fully marked (800 submissions waiting for the HoD), the rest submitted
      db.exec("INSERT INTO submission (assignment_id, student_id, state, submitted_at, days_late, raw_mark) "
              "SELECT a.assignment_id, u.user_id, "
              "CASE WHEN a.assignment_id = 1 THEN 'MARKED' ELSE 'SUBMITTED' END, "
              "'2026-10-30T10:00:00Z', 0, CASE WHEN a.assignment_id = 1 THEN 30 END "
              "FROM assignment a CROSS JOIN app_user u WHERE u.role = 'STUDENT'");
    });
  }
};

TEST_F(PerformanceFixture, CapacityIsStoredWithoutErrors) {  // NFR3
  EXPECT_EQ(countRows(db, "app_user", "role = 'STUDENT'"), 800);
  EXPECT_EQ(countRows(db, "app_user", "role = 'LECTURER'"), 50);
  EXPECT_EQ(countRows(db, "course"), 30);
  EXPECT_GE(countRows(db, "submission"), 25000);
}

TEST_F(PerformanceFixture, LoginWithin2Seconds) {  // NFR1, including hash verification
  const auto start = std::chrono::steady_clock::now();
  auto user = s.auth.login("student800", "password1");
  const double took = secondsSince(start);
  ASSERT_TRUE(user != nullptr);
  EXPECT_LT(took, 2.0) << "login took " << took << " s";
}

TEST_F(PerformanceFixture, StudentAssignmentListWithin1Second) {  // NFR2
  auto student = s.users.findByUsername("student400");
  const auto start = std::chrono::steady_clock::now();
  std::size_t total = 0;
  for (const Course& c : s.assignments.coursesForStudent(*student))
    total += s.assignments.assignmentsForStudent(*student, c.id).size();
  const double took = secondsSince(start);
  EXPECT_EQ(total, 32u);  // every assignment in all 30 courses
  EXPECT_LT(took, 1.0) << "listing took " << took << " s";
}

TEST_F(PerformanceFixture, ModerationListOf800Within3Seconds) {  // NFR2
  auto hod = s.users.findByUsername("hod");
  const auto start = std::chrono::steady_clock::now();
  const auto items = s.moderation.listForModeration(*hod);
  const double took = secondsSince(start);
  EXPECT_EQ(items.size(), 800u);
  EXPECT_LT(took, 3.0) << "moderation list took " << took << " s";
}

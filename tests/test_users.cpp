#include <gtest/gtest.h>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/domain/UserFactory.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AuthService.hpp"

using namespace lms;

TEST(Hasher, SaltedHashAndVerify) {
  LibsodiumHasher h;
  const std::string a = h.hash("secret"), b = h.hash("secret");
  EXPECT_NE(a, "secret");       // never plain text
  EXPECT_NE(a, b);              // different random salt each time
  EXPECT_TRUE(h.verify("secret", a));
  EXPECT_FALSE(h.verify("wrong", a));
}

TEST(UserFactory, BuildsRightSubclassAndEnforcesDepartment) {
  UserData d; d.username = "x"; d.fullName = "X"; d.departmentId = 1;
  EXPECT_EQ(UserFactory::create(Role::Student, d)->role(), Role::Student);
  EXPECT_EQ(UserFactory::create(Role::HeadOfDepartment, d)->role(), Role::HeadOfDepartment);
  d.departmentId.reset();
  EXPECT_THROW(UserFactory::create(Role::Lecturer, d), RuleViolation);
}

TEST(Roles, PermissionsDiffer) {
  UserData d; d.departmentId = 1;
  auto student = UserFactory::create(Role::Student, d);
  auto hod = UserFactory::create(Role::HeadOfDepartment, d);
  EXPECT_TRUE(student->can(Action::SubmitAssignment));
  EXPECT_FALSE(student->can(Action::ReleaseMarks));
  EXPECT_TRUE(hod->can(Action::ReleaseMarks));
  EXPECT_FALSE(hod->can(Action::MarkSubmission));   // HoD is not a Lecturer
}

class DbFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  LibsodiumHasher hasher;
  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    db.exec("INSERT INTO department (name) VALUES ('Science')");
  }
};

TEST_F(DbFixture, LoginSucceedsAndFailsCorrectly) {
  SqliteUserRepository repo(db);
  repo.add({"sam", hasher.hash("pw"), "Sam", Role::Student, std::nullopt});
  AuthService auth(repo, hasher);
  auto u = auth.login("sam", "pw");
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(u->role(), Role::Student);
  EXPECT_EQ(auth.login("sam", "bad"), nullptr);
  EXPECT_EQ(auth.login("nobody", "pw"), nullptr);
}

TEST_F(DbFixture, PasswordIsNotStoredInPlainText) {
  SqliteUserRepository repo(db);
  repo.add({"sam", hasher.hash("pw"), "Sam", Role::Student, std::nullopt});
  auto st = db.prepare("SELECT COUNT(*) FROM app_user WHERE password_hash = ?");
  st.bind(1, std::string("pw"));
  ASSERT_TRUE(st.step());
  EXPECT_EQ(st.columnInt(0), 0);
}

TEST_F(DbFixture, InjectionAttemptIsHarmless) {
  SqliteUserRepository repo(db);
  repo.add({"sam", hasher.hash("pw"), "Sam", Role::Student, std::nullopt});
  EXPECT_EQ(repo.findByUsername("sam' OR '1'='1"), nullptr);   // treated as plain text
}

TEST_F(DbFixture, RulesEnforcedByDatabase) {
  SqliteUserRepository repo(db);
  repo.add({"hod1", "h", "H", Role::HeadOfDepartment, 1});
  EXPECT_THROW(repo.add({"hod2", "h", "H2", Role::HeadOfDepartment, 1}), RuleViolation);  // one HoD per dept
  EXPECT_THROW(repo.add({"hod1", "h", "Dup", Role::Student, std::nullopt}), RuleViolation);  // duplicate username
  EXPECT_THROW(db.exec("INSERT INTO app_user (username,password_hash,full_name,role) VALUES ('l','h','L','LECTURER')"),
               DatabaseError);  // lecturer without department
}

TEST_F(DbFixture, AuditLogIsAppendOnlyAndRollbackWorks) {
  SqliteUserRepository repo(db);
  const int s = repo.add({"sam", "h", "Sam", Role::Student, std::nullopt});
  db.exec("INSERT INTO course (code,name,department_id) VALUES ('C1','Course',1)");
  db.exec("INSERT INTO assignment (course_id,title,deadline,max_mark) VALUES (1,'A1','2026-11-01T10:00:00Z',50)");
  db.exec("INSERT INTO submission (assignment_id,student_id,state,submitted_at,raw_mark) VALUES (1," + std::to_string(s) + ",'MARKED','2026-10-31T10:00:00Z',40)");
  db.exec("INSERT INTO audit_log (submission_id,changed_by,changed_at,old_value,new_value,reason) VALUES (1," + std::to_string(s) + ",'t',NULL,40,'initial marking')");
  EXPECT_THROW(db.exec("UPDATE audit_log SET reason = 'edited'"), DatabaseError);
  EXPECT_THROW(db.exec("DELETE FROM audit_log"), DatabaseError);
  EXPECT_THROW(db.exec("INSERT INTO audit_log (submission_id,changed_by,changed_at,new_value,reason) VALUES (1,1,'t',41,'  ')"), DatabaseError);
  {
    Transaction tx(db);
    db.exec("INSERT INTO department (name) VALUES ('Temp')");
  }  // no commit -> rolled back
  auto st = db.prepare("SELECT COUNT(*) FROM department WHERE name = 'Temp'");
  ASSERT_TRUE(st.step());
  EXPECT_EQ(st.columnInt(0), 0);
}

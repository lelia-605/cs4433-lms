#include <gtest/gtest.h>

#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteCourseRepository.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AdminService.hpp"
#include "lms/services/AuthService.hpp"

using namespace lms;

class AdminFixture : public ::testing::Test {
 protected:
  DatabaseConnection db{":memory:"};
  LibsodiumHasher hasher;
  SqliteUserRepository users{db};
  SqliteCourseRepository courses{db};
  AdminService admin{users, courses, hasher};
  AuthService auth{users, hasher};
  Administrator actor{1000, "root", "x", "Root Admin", true};  // permission check needs no DB row
  int science = 0;

  void SetUp() override {
    db.runScript(LMS_SCHEMA_PATH);
    science = admin.createDepartment(actor, "Science");
  }
};

// ---- FR3 users ----
TEST_F(AdminFixture, CreatesUsersOfAllRolesWhoCanThenLogIn) {
  admin.createUser(actor, "ann", "password1", "Ann A", Role::Administrator, std::nullopt);
  admin.createUser(actor, "lee", "password2", "Lee L", Role::Lecturer, science);
  admin.createUser(actor, "hal", "password3", "Hal H", Role::HeadOfDepartment, science);
  admin.createUser(actor, "sam", "password4", "Sam S", Role::Student, std::nullopt);
  EXPECT_EQ(auth.login("lee", "password2")->role(), Role::Lecturer);
  EXPECT_EQ(auth.login("hal", "password3")->role(), Role::HeadOfDepartment);
  EXPECT_EQ(auth.login("sam", "password4")->menuTitle(), "Student menu");
}

TEST_F(AdminFixture, OnlyAdministratorsMayManageUsers) {
  Student student(5, "s", "h", "S", true);
  EXPECT_THROW(admin.createUser(student, "x", "password1", "X", Role::Student, std::nullopt), AccessDenied);
  EXPECT_THROW(admin.createDepartment(student, "Arts"), AccessDenied);
  EXPECT_THROW(admin.enrolStudent(student, 1, 1), AccessDenied);
}

TEST_F(AdminFixture, RejectsInvalidInput) {
  EXPECT_THROW(admin.createUser(actor, "", "password1", "X", Role::Student, std::nullopt), RuleViolation);
  EXPECT_THROW(admin.createUser(actor, "a b", "password1", "X", Role::Student, std::nullopt), RuleViolation);
  EXPECT_THROW(admin.createUser(actor, "x", "short", "X", Role::Student, std::nullopt), RuleViolation);
  EXPECT_THROW(admin.createUser(actor, "x", "password1", "   ", Role::Student, std::nullopt), RuleViolation);
  EXPECT_THROW(admin.createUser(actor, "x", "password1", "X", Role::Lecturer, std::nullopt), RuleViolation);
  EXPECT_THROW(admin.createUser(actor, "x", "password1", "X", Role::Lecturer, 999), NotFound);
  EXPECT_THROW(admin.createUser(actor, std::string(51, 'a'), "password1", "X", Role::Student, std::nullopt), RuleViolation);
}

TEST_F(AdminFixture, RejectsDuplicateUsername) {
  admin.createUser(actor, "sam", "password1", "Sam", Role::Student, std::nullopt);
  EXPECT_THROW(admin.createUser(actor, "sam", "password2", "Other", Role::Student, std::nullopt), RuleViolation);
}

TEST_F(AdminFixture, DeactivatedUserCannotLogIn) {
  const int id = admin.createUser(actor, "sam", "password1", "Sam", Role::Student, std::nullopt);
  ASSERT_NE(auth.login("sam", "password1"), nullptr);
  admin.deactivateUser(actor, id);
  EXPECT_EQ(auth.login("sam", "password1"), nullptr);
  EXPECT_THROW(admin.deactivateUser(actor, 9999), NotFound);
}

TEST_F(AdminFixture, AdminCannotDeactivateOwnAccount) {
  const int id = admin.createUser(actor, "ann", "password1", "Ann", Role::Administrator, std::nullopt);
  auto self = users.findById(id);
  EXPECT_THROW(admin.deactivateUser(*self, id), RuleViolation);
}

TEST_F(AdminFixture, EditsUserProfile) {
  const int id = admin.createUser(actor, "sam", "password1", "Sam", Role::Student, std::nullopt);
  admin.editUser(actor, id, "Samuel Student", std::nullopt);
  EXPECT_EQ(users.findById(id)->fullName(), "Samuel Student");
  EXPECT_THROW(admin.editUser(actor, 9999, "X", std::nullopt), NotFound);
}

// ---- FR22 password reset ----
TEST_F(AdminFixture, ResetPasswordChangesWhichPasswordWorks) {
  const int id = admin.createUser(actor, "sam", "password1", "Sam", Role::Student, std::nullopt);
  admin.resetPassword(actor, id, "newpassword");
  EXPECT_EQ(auth.login("sam", "password1"), nullptr);
  EXPECT_NE(auth.login("sam", "newpassword"), nullptr);
  EXPECT_THROW(admin.resetPassword(actor, id, "short"), RuleViolation);
}

// ---- FR4 departments and courses ----
TEST_F(AdminFixture, CreatesCoursesAndRejectsDuplicates) {
  const int c = admin.createCourse(actor, "CS101", "Intro", science);
  EXPECT_EQ(courses.findCourse(c)->name, "Intro");
  EXPECT_THROW(admin.createCourse(actor, "CS101", "Again", science), RuleViolation);   // duplicate code
  EXPECT_THROW(admin.createCourse(actor, "CS102", "X", 999), NotFound);                // no such department
  EXPECT_THROW(admin.createDepartment(actor, "Science"), RuleViolation);               // duplicate name
  admin.renameCourse(actor, c, "Introduction");
  EXPECT_EQ(courses.findCourse(c)->name, "Introduction");
  EXPECT_THROW(admin.renameCourse(actor, 999, "X"), NotFound);
}

// ---- FR5 lecturer assignment and enrolment ----
TEST_F(AdminFixture, AssignsOnlyLecturersToCourses) {
  const int course = admin.createCourse(actor, "CS101", "Intro", science);
  const int lecturer = admin.createUser(actor, "lee", "password1", "Lee", Role::Lecturer, science);
  const int student = admin.createUser(actor, "sam", "password2", "Sam", Role::Student, std::nullopt);
  admin.assignLecturer(actor, course, lecturer);
  EXPECT_EQ(courses.findCourse(course)->lecturerId, lecturer);
  EXPECT_THROW(admin.assignLecturer(actor, course, student), RuleViolation);
  EXPECT_THROW(admin.assignLecturer(actor, 999, lecturer), NotFound);
  EXPECT_THROW(admin.assignLecturer(actor, course, 999), NotFound);
}

TEST_F(AdminFixture, EnrolsOnlyActiveStudentsOnce) {
  const int course = admin.createCourse(actor, "CS101", "Intro", science);
  const int student = admin.createUser(actor, "sam", "password1", "Sam", Role::Student, std::nullopt);
  const int lecturer = admin.createUser(actor, "lee", "password2", "Lee", Role::Lecturer, science);
  EXPECT_FALSE(courses.isEnrolled(student, course));
  admin.enrolStudent(actor, student, course);
  EXPECT_TRUE(courses.isEnrolled(student, course));
  EXPECT_THROW(admin.enrolStudent(actor, student, course), RuleViolation);   // twice
  EXPECT_THROW(admin.enrolStudent(actor, lecturer, course), RuleViolation);  // not a student
  EXPECT_THROW(admin.enrolStudent(actor, student, 999), NotFound);
  const int s2 = admin.createUser(actor, "tom", "password3", "Tom", Role::Student, std::nullopt);
  admin.deactivateUser(actor, s2);
  EXPECT_THROW(admin.enrolStudent(actor, s2, course), RuleViolation);        // deactivated
}

#pragma once
#include <optional>
#include <string>

#include "lms/data/ICourseRepository.hpp"
#include "lms/data/IUserRepository.hpp"
#include "lms/security/IPasswordHasher.hpp"

namespace lms {

// Administrator features: users (FR3, FR22), departments and courses (FR4),
// lecturer assignment and enrolment (FR5). Every operation checks the actor's
// permission first (FR2, NFR6) and validates its input (NFR7).
class AdminService {
 public:
  AdminService(IUserRepository& users, ICourseRepository& courses, const IPasswordHasher& hasher)
      : users_(users), courses_(courses), hasher_(hasher) {}

  int createUser(const User& actor, const std::string& username, const std::string& password,
                 const std::string& fullName, Role role, std::optional<int> departmentId);
  void editUser(const User& actor, int userId, const std::string& fullName,
                std::optional<int> departmentId);
  void deactivateUser(const User& actor, int userId);
  void resetPassword(const User& actor, int userId, const std::string& newPassword);

  int createDepartment(const User& actor, const std::string& name);
  int createCourse(const User& actor, const std::string& code, const std::string& name,
                   int departmentId);
  void renameCourse(const User& actor, int courseId, const std::string& name);

  void assignLecturer(const User& actor, int courseId, int lecturerId);
  void enrolStudent(const User& actor, int studentId, int courseId);

  static constexpr std::size_t kMinPasswordLength = 8;

 private:
  static void require(const User& actor, Action action);
  static void requireText(const std::string& value, const char* field, std::size_t maxLength);
  void requirePassword(const std::string& password) const;

  IUserRepository& users_;
  ICourseRepository& courses_;
  const IPasswordHasher& hasher_;
};

}  // namespace lms

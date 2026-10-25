#pragma once
#include <string>

#include "lms/Errors.hpp"

namespace lms {

enum class Role { Administrator, Lecturer, HeadOfDepartment, Student };

enum class Action {
  ManageUsers, ManageCourses, AssignAndEnrol,
  CreateAssignment, GrantExtension, MarkSubmission, ViewSubmissionStatus,
  ModerateMarks, ReleaseMarks, ViewAuditLog,
  ViewCourses, SubmitAssignment, ViewReleasedMarks
};

// Role <-> text stored in app_user.role (matches the CHECK constraint).
inline std::string roleToString(Role r) {
  switch (r) {
    case Role::Administrator:    return "ADMIN";
    case Role::Lecturer:         return "LECTURER";
    case Role::HeadOfDepartment: return "HOD";
    case Role::Student:          return "STUDENT";
  }
  throw RuleViolation("unknown role");
}

inline Role roleFromString(const std::string& s) {
  if (s == "ADMIN")    return Role::Administrator;
  if (s == "LECTURER") return Role::Lecturer;
  if (s == "HOD")      return Role::HeadOfDepartment;
  if (s == "STUDENT")  return Role::Student;
  throw RuleViolation("unknown role: " + s);
}

}  // namespace lms

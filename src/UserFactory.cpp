#include "lms/domain/UserFactory.hpp"

namespace lms {

std::unique_ptr<User> UserFactory::create(Role role, const UserData& d) {
  switch (role) {
    case Role::Administrator:
      return std::make_unique<Administrator>(d.id, d.username, d.passwordHash, d.fullName, d.active);
    case Role::Student:
      return std::make_unique<Student>(d.id, d.username, d.passwordHash, d.fullName, d.active);
    case Role::Lecturer:
      if (!d.departmentId) throw RuleViolation("a lecturer needs a department");
      return std::make_unique<Lecturer>(d.id, d.username, d.passwordHash, d.fullName, d.active, *d.departmentId);
    case Role::HeadOfDepartment:
      if (!d.departmentId) throw RuleViolation("a head of department needs a department");
      return std::make_unique<HeadOfDepartment>(d.id, d.username, d.passwordHash, d.fullName, d.active, *d.departmentId);
  }
  throw RuleViolation("unknown role");
}

}  // namespace lms

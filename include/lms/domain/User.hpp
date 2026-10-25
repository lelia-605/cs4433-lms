#pragma once
#include <set>
#include <string>

#include "lms/domain/Role.hpp"
#include "lms/security/IPasswordHasher.hpp"

namespace lms {

// Abstract base class: there is no such thing as a user without a role.
class User {
 public:
  User(int id, std::string username, std::string passwordHash, std::string fullName, bool active)
      : id_(id), username_(std::move(username)), passwordHash_(std::move(passwordHash)),
        fullName_(std::move(fullName)), active_(active) {}
  virtual ~User() = default;  // needed to delete a subclass through a User pointer

  // Same for every role, so written once here.
  bool verifyPassword(const std::string& plain, const IPasswordHasher& hasher) const {
    return active_ && hasher.verify(plain, passwordHash_);
  }
  bool can(Action a) const { return allowedActions().count(a) > 0; }

  int id() const { return id_; }
  const std::string& username() const { return username_; }
  const std::string& fullName() const { return fullName_; }
  bool active() const { return active_; }

  // Different for every role: this is the polymorphism the brief asks for.
  virtual Role role() const = 0;
  virtual std::set<Action> allowedActions() const = 0;
  virtual std::string menuTitle() const = 0;

 private:
  int id_;
  std::string username_, passwordHash_, fullName_;
  bool active_;
};

class Administrator : public User {
 public:
  using User::User;
  Role role() const override { return Role::Administrator; }
  std::set<Action> allowedActions() const override {
    return {Action::ManageUsers, Action::ManageCourses, Action::AssignAndEnrol};
  }
  std::string menuTitle() const override { return "Administrator menu"; }
};

class Lecturer : public User {
 public:
  Lecturer(int id, std::string u, std::string h, std::string n, bool a, int departmentId)
      : User(id, std::move(u), std::move(h), std::move(n), a), departmentId_(departmentId) {}
  int departmentId() const { return departmentId_; }
  Role role() const override { return Role::Lecturer; }
  std::set<Action> allowedActions() const override {
    return {Action::CreateAssignment, Action::GrantExtension, Action::MarkSubmission,
            Action::ViewSubmissionStatus};
  }
  std::string menuTitle() const override { return "Lecturer menu"; }

 private:
  int departmentId_;
};

// Not a subclass of Lecturer: its rights are different, not a superset.
class HeadOfDepartment : public User {
 public:
  HeadOfDepartment(int id, std::string u, std::string h, std::string n, bool a, int departmentId)
      : User(id, std::move(u), std::move(h), std::move(n), a), departmentId_(departmentId) {}
  int departmentId() const { return departmentId_; }
  Role role() const override { return Role::HeadOfDepartment; }
  std::set<Action> allowedActions() const override {
    return {Action::ModerateMarks, Action::ReleaseMarks, Action::ViewAuditLog};
  }
  std::string menuTitle() const override { return "Head of Department menu"; }

 private:
  int departmentId_;
};

class Student : public User {
 public:
  using User::User;
  Role role() const override { return Role::Student; }
  std::set<Action> allowedActions() const override {
    return {Action::ViewCourses, Action::SubmitAssignment, Action::ViewReleasedMarks};
  }
  std::string menuTitle() const override { return "Student menu"; }
};

}  // namespace lms

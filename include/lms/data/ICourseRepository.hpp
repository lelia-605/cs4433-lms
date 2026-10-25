#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/domain/Course.hpp"

namespace lms {

// Repository for departments, courses and enrolments.
class ICourseRepository {
 public:
  virtual ~ICourseRepository() = default;
  virtual int addDepartment(const std::string& name) = 0;
  virtual std::optional<Department> findDepartment(int id) const = 0;
  virtual void renameDepartment(int id, const std::string& name) = 0;
  virtual int addCourse(const Course& c) = 0;  // c.id is ignored
  virtual std::optional<Course> findCourse(int id) const = 0;
  virtual void renameCourse(int id, const std::string& name) = 0;
  virtual void setLecturer(int courseId, int lecturerId) = 0;
  virtual void enrol(int studentId, int courseId) = 0;
  virtual bool isEnrolled(int studentId, int courseId) const = 0;
  virtual std::vector<Course> coursesForStudent(int studentId) const = 0;
  virtual std::vector<Course> coursesForLecturer(int lecturerId) const = 0;
  virtual std::vector<int> enrolledStudents(int courseId) const = 0;  // student ids, in id order
  virtual std::vector<Department> allDepartments() const = 0;
  virtual std::vector<Course> allCourses() const = 0;
  virtual std::vector<Course> coursesInDepartment(int departmentId) const = 0;
};

}  // namespace lms

#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/ICourseRepository.hpp"

namespace lms {

class SqliteCourseRepository : public ICourseRepository {
 public:
  explicit SqliteCourseRepository(DatabaseConnection& db) : db_(db) {}
  int addDepartment(const std::string& name) override;
  std::optional<Department> findDepartment(int id) const override;
  int addCourse(const Course& c) override;
  std::optional<Course> findCourse(int id) const override;
  void renameCourse(int id, const std::string& name) override;
  void setLecturer(int courseId, int lecturerId) override;
  void enrol(int studentId, int courseId) override;
  bool isEnrolled(int studentId, int courseId) const override;

 private:
  DatabaseConnection& db_;
};

}  // namespace lms

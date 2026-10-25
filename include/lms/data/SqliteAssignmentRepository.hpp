#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/IAssignmentRepository.hpp"

namespace lms {

class SqliteAssignmentRepository : public IAssignmentRepository {
 public:
  explicit SqliteAssignmentRepository(DatabaseConnection& db) : db_(db) {}
  int addAssignment(const Assignment& a) override;
  std::optional<Assignment> findAssignment(int id) const override;
  std::vector<Assignment> findByCourse(int courseId) const override;
  void saveExtension(const Extension& e) override;
  std::optional<Extension> findExtension(int assignmentId, int studentId) const override;

 private:
  DatabaseConnection& db_;
};

}  // namespace lms

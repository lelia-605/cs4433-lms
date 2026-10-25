#pragma once
#include <optional>
#include <vector>

#include "lms/domain/Assignment.hpp"

namespace lms {

class IAssignmentRepository {
 public:
  virtual ~IAssignmentRepository() = default;
  virtual int addAssignment(const Assignment& a) = 0;  // a.id is ignored
  virtual std::optional<Assignment> findAssignment(int id) const = 0;
  virtual std::vector<Assignment> findByCourse(int courseId) const = 0;
  virtual void saveExtension(const Extension& e) = 0;  // inserts, or replaces the student's earlier one
  virtual std::optional<Extension> findExtension(int assignmentId, int studentId) const = 0;
};

}  // namespace lms

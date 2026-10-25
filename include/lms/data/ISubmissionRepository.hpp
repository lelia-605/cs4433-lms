#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/domain/Submission.hpp"

namespace lms {

// Repository for submissions and their versions.
class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  virtual std::optional<Submission> find(int assignmentId, int studentId) const = 0;
  // Creates the submission or updates it, and adds a new version, in one transaction.
  // Returns the new version number.
  virtual int recordVersion(const Submission& s, const std::string& content) = 0;
  virtual std::vector<SubmissionVersion> versions(int submissionId) const = 0;
  virtual std::vector<Submission> findByAssignment(int assignmentId) const = 0;
};

}  // namespace lms

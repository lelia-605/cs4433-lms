#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/domain/Submission.hpp"

namespace lms {

// Repository for submissions, their versions and marks. The audit log has its own
// repository (IAuditRepository) and is written by AuditObserver.
class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  virtual std::optional<Submission> find(int assignmentId, int studentId) const = 0;
  virtual std::optional<Submission> findById(int submissionId) const = 0;
  // Creates the submission or updates it, and adds a new version, in one transaction.
  // Returns the new version number (FR9, FR10).
  virtual int recordVersion(const Submission& s, const std::string& content) = 0;
  virtual std::vector<SubmissionVersion> versions(int submissionId) const = 0;
  virtual std::vector<Submission> findByAssignment(int assignmentId) const = 0;
  // MARKED submissions on courses in this department, oldest first (FR15).
  virtual std::vector<Submission> awaitingModeration(int departmentId) const = 0;

  // Mark and moderation data. The state itself is saved by StatusObserver through setState.
  virtual void saveMark(int submissionId, int rawMark, const std::string& feedback, int markedBy) = 0;
  virtual void saveModeratedMark(int submissionId, int rawMark, int moderatedBy) = 0;
  virtual void setReleasedAt(int submissionId, TimePoint at) = 0;
  virtual void setState(int submissionId, SubmissionState state) = 0;
};

}  // namespace lms

#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/domain/AuditEntry.hpp"
#include "lms/domain/Submission.hpp"

namespace lms {

// Repository for submissions, their versions, marks and the audit log.
class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  virtual std::optional<Submission> find(int assignmentId, int studentId) const = 0;
  // Creates the submission or updates it, and adds a new version, in one transaction.
  // Returns the new version number.
  virtual int recordVersion(const Submission& s, const std::string& content) = 0;
  virtual std::vector<SubmissionVersion> versions(int submissionId) const = 0;
  virtual std::vector<Submission> findByAssignment(int assignmentId) const = 0;
  // Saves the mark and feedback, sets the state to MARKED and appends the audit
  // entry, all in one transaction (FR13, FR14, NFR11).
  virtual void saveMark(int submissionId, int rawMark, const std::string& feedback,
                        const AuditEntry& entry) = 0;
  virtual std::vector<AuditEntry> auditTrail(int submissionId) const = 0;
};

}  // namespace lms

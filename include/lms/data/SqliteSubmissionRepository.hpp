#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/ISubmissionRepository.hpp"

namespace lms {

class SqliteSubmissionRepository : public ISubmissionRepository {
 public:
  explicit SqliteSubmissionRepository(DatabaseConnection& db) : db_(db) {}
  std::optional<Submission> find(int assignmentId, int studentId) const override;
  int recordVersion(const Submission& s, const std::string& content) override;
  std::vector<SubmissionVersion> versions(int submissionId) const override;
  std::vector<Submission> findByAssignment(int assignmentId) const override;
  void saveMark(int submissionId, int rawMark, const std::string& feedback,
                const AuditEntry& entry) override;
  std::vector<AuditEntry> auditTrail(int submissionId) const override;
  std::optional<Submission> findById(int submissionId) const override;
  std::vector<Submission> awaitingModeration(int departmentId) const override;
  void saveModeration(int submissionId, int rawMark, const AuditEntry& entry) override;
  int releaseModerated(int assignmentId, TimePoint at) override;

 private:
  int upsertSubmission(const Submission& s);
  void appendAudit(const AuditEntry& entry);
  DatabaseConnection& db_;
};

}  // namespace lms

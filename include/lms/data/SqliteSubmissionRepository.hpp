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

 private:
  int upsertSubmission(const Submission& s);
  DatabaseConnection& db_;
};

}  // namespace lms

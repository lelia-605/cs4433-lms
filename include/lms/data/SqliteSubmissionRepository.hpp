#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/ISubmissionRepository.hpp"

namespace lms {

class SqliteSubmissionRepository : public ISubmissionRepository {
 public:
  explicit SqliteSubmissionRepository(DatabaseConnection& db) : db_(db) {}
  std::optional<Submission> find(int assignmentId, int studentId) const override;
  std::optional<Submission> findById(int submissionId) const override;
  int recordVersion(const Submission& s, const std::string& content) override;
  std::vector<SubmissionVersion> versions(int submissionId) const override;
  std::vector<Submission> findByAssignment(int assignmentId) const override;
  std::vector<Submission> awaitingModeration(int departmentId) const override;
  void saveMark(int submissionId, int rawMark, const std::string& feedback, int markedBy) override;
  void saveModeratedMark(int submissionId, int rawMark, int moderatedBy) override;
  void setReleasedAt(int submissionId, TimePoint at) override;
  void setState(int submissionId, SubmissionState state) override;

 private:
  int upsertSubmission(const Submission& s);
  void runUpdate(Statement& st);
  DatabaseConnection& db_;
};

}  // namespace lms

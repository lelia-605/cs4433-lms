#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/IAuditRepository.hpp"

namespace lms {

class SqliteAuditRepository : public IAuditRepository {
 public:
  explicit SqliteAuditRepository(DatabaseConnection& db) : db_(db) {}
  void append(const AuditEntry& entry) override;
  std::vector<AuditEntry> forSubmission(int submissionId) const override;

 private:
  DatabaseConnection& db_;
};

}  // namespace lms

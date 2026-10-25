#pragma once
#include <vector>

#include "lms/domain/AuditEntry.hpp"

namespace lms {

// The append-only audit log (FR19, FR20, NFR11). There is no update or delete.
class IAuditRepository {
 public:
  virtual ~IAuditRepository() = default;
  virtual void append(const AuditEntry& entry) = 0;
  virtual std::vector<AuditEntry> forSubmission(int submissionId) const = 0;  // oldest first
};

}  // namespace lms

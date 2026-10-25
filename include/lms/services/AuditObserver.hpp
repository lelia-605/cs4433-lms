#pragma once
#include "lms/data/IAuditRepository.hpp"
#include "lms/services/ISubmissionObserver.hpp"

namespace lms {

// Writes one audit entry for every mark creation or change (FR19, BR6, NFR11).
class AuditObserver : public ISubmissionObserver {
 public:
  explicit AuditObserver(IAuditRepository& audit) : audit_(audit) {}
  void onMarkChanged(const AuditEntry& change) override { audit_.append(change); }
  void onStateChanged(int, SubmissionState, SubmissionState) override {}  // not a mark change

 private:
  IAuditRepository& audit_;
};

}  // namespace lms

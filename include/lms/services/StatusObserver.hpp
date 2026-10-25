#pragma once
#include "lms/data/ISubmissionRepository.hpp"
#include "lms/services/ISubmissionObserver.hpp"

namespace lms {

// Saves each new submission state, which is the status lecturers, the HoD and
// students see (FR14, FR21).
class StatusObserver : public ISubmissionObserver {
 public:
  explicit StatusObserver(ISubmissionRepository& submissions) : submissions_(submissions) {}
  void onMarkChanged(const AuditEntry&) override {}  // the state does not change
  void onStateChanged(int submissionId, SubmissionState, SubmissionState to) override {
    submissions_.setState(submissionId, to);
  }

 private:
  ISubmissionRepository& submissions_;
};

}  // namespace lms

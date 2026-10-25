#pragma once
#include <vector>

#include "lms/domain/AuditEntry.hpp"
#include "lms/domain/Submission.hpp"

namespace lms {

// Observer pattern: services report every mark change and state change, and each
// observer reacts in its own way. A new reaction (e.g. a notification) is a new
// observer, with no change to the services (Open/Closed).
class ISubmissionObserver {
 public:
  virtual ~ISubmissionObserver() = default;
  virtual void onMarkChanged(const AuditEntry& change) = 0;
  virtual void onStateChanged(int submissionId, SubmissionState from, SubmissionState to) = 0;
};

// The subject side: keeps the attached observers and notifies them.
// MarkingService and ModerationService inherit from it.
class SubmissionSubject {
 public:
  void attach(ISubmissionObserver& observer) { observers_.push_back(&observer); }

 protected:
  void notifyMarkChanged(const AuditEntry& change) const {
    for (ISubmissionObserver* o : observers_) o->onMarkChanged(change);
  }
  void notifyStateChanged(int submissionId, SubmissionState from, SubmissionState to) const {
    for (ISubmissionObserver* o : observers_) o->onStateChanged(submissionId, from, to);
  }

 private:
  std::vector<ISubmissionObserver*> observers_;  // not owned; they live as long as the service
};

}  // namespace lms

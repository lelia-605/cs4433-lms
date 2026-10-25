#pragma once
#include <functional>

namespace lms {

// Runs some work as one database transaction: everything is saved, or (if anything
// throws) nothing is. Lets a service and its observers share one transaction
// without the service knowing about SQL (NFR11, UC09 E5).
class ITransactionRunner {
 public:
  virtual ~ITransactionRunner() = default;
  virtual void runInTransaction(const std::function<void()>& work) = 0;
};

}  // namespace lms

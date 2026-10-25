#include "lms/data/SqliteAuditRepository.hpp"

namespace lms {

void SqliteAuditRepository::append(const AuditEntry& entry) {
  auto st = db_.prepare(
      "INSERT INTO audit_log (submission_id, changed_by, changed_at, old_value, new_value, reason) "
      "VALUES (?, ?, ?, ?, ?, ?)");
  st.bind(1, entry.submissionId);
  st.bind(2, entry.changedBy);
  st.bind(3, toIso(entry.changedAt));
  if (entry.oldValue) st.bind(4, *entry.oldValue); else st.bindNull(4);
  if (entry.newValue) st.bind(5, *entry.newValue); else st.bindNull(5);
  st.bind(6, entry.reason);
  st.step();
}

std::vector<AuditEntry> SqliteAuditRepository::forSubmission(int submissionId) const {
  auto st = db_.prepare(
      "SELECT audit_id, submission_id, changed_by, changed_at, old_value, new_value, reason "
      "FROM audit_log WHERE submission_id = ? ORDER BY audit_id");
  st.bind(1, submissionId);
  std::vector<AuditEntry> out;
  while (st.step()) {
    AuditEntry e;
    e.id = st.columnInt(0);
    e.submissionId = st.columnInt(1);
    e.changedBy = st.columnInt(2);
    e.changedAt = fromIso(st.columnText(3));
    if (!st.columnIsNull(4)) e.oldValue = st.columnInt(4);
    if (!st.columnIsNull(5)) e.newValue = st.columnInt(5);
    e.reason = st.columnText(6);
    out.push_back(e);
  }
  return out;
}

}  // namespace lms

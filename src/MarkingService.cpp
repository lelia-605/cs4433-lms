#include "lms/services/MarkingService.hpp"

#include <algorithm>
#include <cctype>

namespace lms {

namespace {
bool isBlank(const std::string& s) {
  return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}
}  // namespace

MarkDetails buildMarkDetails(const Submission& s, const Assignment& a, const IPenaltyStrategy& penalty) {
  MarkDetails d;
  d.rawMark = s.rawMark.value_or(0);
  d.maxMark = a.maxMark;
  d.daysLate = s.daysLate;
  d.finalMark = finalMark(d.rawMark, a.maxMark, s.daysLate, penalty);
  d.penalty = d.rawMark - d.finalMark;  // what was actually taken off
  d.feedback = s.feedback;
  d.state = s.state;
  return d;
}

Assignment MarkingService::lecturersAssignment(const User& lecturer, int assignmentId) const {
  if (!lecturer.can(Action::MarkSubmission)) throw AccessDenied("you are not allowed to do this");
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  auto course = courses_.findCourse(assignment->courseId);
  if (!course || course->lecturerId != lecturer.id()) throw AccessDenied("you do not teach this course");
  return *assignment;
}

Submission MarkingService::submissionOf(int assignmentId, int studentId) const {
  auto s = submissions_.find(assignmentId, studentId);
  if (!s) throw NotFound("this student has not submitted");
  return *s;
}

void MarkingService::checkMarkAndFeedback(const Assignment& a, int rawMark,
                                          const std::string& feedback) const {
  if (rawMark < 0 || rawMark > a.maxMark)
    throw RuleViolation("the mark must be between 0 and " + std::to_string(a.maxMark));
  if (isBlank(feedback)) throw RuleViolation("feedback is required");
  if (feedback.size() > kMaxFeedbackLength)
    throw RuleViolation("feedback is too long (maximum " + std::to_string(kMaxFeedbackLength) + ")");
}

void MarkingService::markSubmission(const User& lecturer, int assignmentId, int studentId,
                                    int rawMark, const std::string& feedback) {
  const Assignment a = lecturersAssignment(lecturer, assignmentId);
  const Submission s = submissionOf(assignmentId, studentId);
  if (s.rawMark) throw RuleViolation("already marked: change the mark and give a reason instead");
  checkMarkAndFeedback(a, rawMark, feedback);
  AuditEntry entry;
  entry.submissionId = s.id;
  entry.changedBy = lecturer.id();
  entry.changedAt = clock_.now();
  entry.newValue = rawMark;
  entry.reason = "initial marking";
  submissions_.saveMark(s.id, rawMark, feedback, entry);
}

void MarkingService::changeMark(const User& lecturer, int assignmentId, int studentId,
                                int newRawMark, const std::string& feedback,
                                const std::string& reason) {
  const Assignment a = lecturersAssignment(lecturer, assignmentId);
  const Submission s = submissionOf(assignmentId, studentId);
  if (!s.rawMark) throw RuleViolation("not marked yet: mark it first");
  // After moderation only the Head of Department may change the mark (slice 5).
  if (s.state != SubmissionState::Marked) throw RuleViolation("the mark has already been moderated");
  if (isBlank(reason)) throw RuleViolation("a reason is required to change a mark");
  checkMarkAndFeedback(a, newRawMark, feedback);
  AuditEntry entry;
  entry.submissionId = s.id;
  entry.changedBy = lecturer.id();
  entry.changedAt = clock_.now();
  entry.oldValue = s.rawMark;
  entry.newValue = newRawMark;
  entry.reason = reason;
  submissions_.saveMark(s.id, newRawMark, feedback, entry);
}

MarkDetails MarkingService::markDetails(const User& lecturer, int assignmentId, int studentId) const {
  const Assignment a = lecturersAssignment(lecturer, assignmentId);
  const Submission s = submissionOf(assignmentId, studentId);
  if (!s.rawMark) throw RuleViolation("not marked yet");
  return buildMarkDetails(s, a, penalty_);
}

std::vector<AuditEntry> MarkingService::auditTrail(const User& viewer, int assignmentId,
                                                   int studentId) const {
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  auto course = courses_.findCourse(assignment->courseId);
  if (!course) throw NotFound("course not found");
  bool allowed = false;
  if (const auto* hod = dynamic_cast<const HeadOfDepartment*>(&viewer))
    allowed = hod->can(Action::ViewAuditLog) && hod->departmentId() == course->departmentId;
  else
    allowed = viewer.can(Action::MarkSubmission) && course->lecturerId == viewer.id();
  if (!allowed) throw AccessDenied("you are not allowed to see this audit trail");
  return submissions_.auditTrail(submissionOf(assignmentId, studentId).id);
}

}  // namespace lms

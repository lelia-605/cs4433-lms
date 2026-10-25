#include "lms/services/MarkingService.hpp"

#include "lms/services/InputValidator.hpp"

namespace lms {

MarkDetails buildMarkDetails(const Submission& s, const Assignment& a, const IPenaltyStrategy& penalty) {
  MarkDetails d;
  d.rawMark = s.rawMark.value_or(0);
  d.maxMark = a.maxMark;
  d.daysLate = s.daysLate;
  d.finalMark = s.finalMark(a.maxMark, penalty);
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
  InputValidator::requireRange(rawMark, 0, a.maxMark, "the mark");
  if (InputValidator::isBlank(feedback)) throw RuleViolation("feedback is required");
  InputValidator::requireText(feedback, "feedback", kMaxFeedbackLength);
}

AuditEntry MarkingService::changeEntry(const User& by, const Submission& s, int newValue,
                                       const std::string& reason) const {
  AuditEntry entry;
  entry.submissionId = s.id;
  entry.changedBy = by.id();
  entry.changedAt = clock_.now();
  entry.oldValue = s.rawMark;  // empty for the first marking
  entry.newValue = newValue;
  entry.reason = reason;
  return entry;
}

void MarkingService::markSubmission(const User& lecturer, int assignmentId, int studentId,
                                    int rawMark, const std::string& feedback) {
  const Assignment a = lecturersAssignment(lecturer, assignmentId);
  Submission s = submissionOf(assignmentId, studentId);
  if (s.rawMark) throw RuleViolation("already marked: change the mark and give a reason instead");
  checkMarkAndFeedback(a, rawMark, feedback);
  const SubmissionState before = s.state;
  s.transitionTo(SubmissionState::Marked);  // throws if the move is not allowed
  const AuditEntry entry = changeEntry(lecturer, s, rawMark, "initial marking");
  transactions_.runInTransaction([&] {
    submissions_.saveMark(s.id, rawMark, feedback, lecturer.id());
    notifyMarkChanged(entry);                    // AuditObserver writes the audit entry
    notifyStateChanged(s.id, before, s.state);   // StatusObserver saves MARKED
  });
}

void MarkingService::changeMark(const User& lecturer, int assignmentId, int studentId,
                                int newRawMark, const std::string& feedback,
                                const std::string& reason) {
  const Assignment a = lecturersAssignment(lecturer, assignmentId);
  const Submission s = submissionOf(assignmentId, studentId);
  if (!s.rawMark) throw RuleViolation("not marked yet: mark it first");
  // After moderation only the Head of Department may change the mark (FR16).
  if (s.state != SubmissionState::Marked) throw RuleViolation("the mark has already been moderated");
  if (InputValidator::isBlank(reason)) throw RuleViolation("a reason is required to change a mark");
  checkMarkAndFeedback(a, newRawMark, feedback);
  const AuditEntry entry = changeEntry(lecturer, s, newRawMark, reason);
  transactions_.runInTransaction([&] {
    submissions_.saveMark(s.id, newRawMark, feedback, lecturer.id());
    notifyMarkChanged(entry);
  });
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
  else if (viewer.role() == Role::Administrator)
    allowed = viewer.can(Action::ViewAuditLog);  // FR20: any submission
  else
    allowed = viewer.can(Action::MarkSubmission) && course->lecturerId == viewer.id();
  if (!allowed) throw AccessDenied("you are not allowed to see this audit trail");
  return audit_.forSubmission(submissionOf(assignmentId, studentId).id);
}

}  // namespace lms

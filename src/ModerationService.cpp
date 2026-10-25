#include "lms/services/ModerationService.hpp"

#include "lms/services/InputValidator.hpp"

namespace lms {

const HeadOfDepartment& ModerationService::asHod(const User& user, Action action) const {
  const auto* hod = dynamic_cast<const HeadOfDepartment*>(&user);
  if (!hod || !hod->can(action)) throw AccessDenied("only a Head of Department can do this");
  return *hod;
}

Assignment ModerationService::departmentAssignment(const HeadOfDepartment& hod, int assignmentId) const {
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  auto course = courses_.findCourse(assignment->courseId);
  if (!course || course->departmentId != hod.departmentId())
    throw AccessDenied("this course is not in your department");
  return *assignment;
}

Submission ModerationService::markedSubmission(const HeadOfDepartment& hod, int submissionId,
                                               Assignment& out) const {
  auto s = submissions_.findById(submissionId);
  if (!s) throw NotFound("submission not found");
  out = departmentAssignment(hod, s->assignmentId);
  if (s->state != SubmissionState::Marked)
    throw RuleViolation("only marked submissions that are not yet moderated can be moderated");
  return *s;
}

std::vector<ModerationItem> ModerationService::listForModeration(const User& user) const {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  std::vector<ModerationItem> out;
  for (const Submission& s : submissions_.awaitingModeration(hod.departmentId())) {
    auto a = assignments_.findAssignment(s.assignmentId);
    if (!a) continue;
    ModerationItem item{s, *a, buildMarkDetails(s, *a, penalty_), std::nullopt, std::nullopt};
    if (auto course = courses_.findCourse(a->courseId)) item.lecturerId = course->lecturerId;
    item.extension = assignments_.findExtension(a->id, s.studentId);
    out.push_back(item);
  }
  return out;
}

// Approve and adjust both end here: save the mark, then tell the observers, all in
// one transaction (AuditObserver writes the entry, StatusObserver saves MODERATED).
void ModerationService::moderate(const HeadOfDepartment& hod, Submission s, int newRawMark,
                                 const std::string& reason) {
  const SubmissionState before = s.state;
  s.transitionTo(SubmissionState::Moderated);
  AuditEntry entry;
  entry.submissionId = s.id;
  entry.changedBy = hod.id();
  entry.changedAt = clock_.now();
  entry.oldValue = s.rawMark;
  entry.newValue = newRawMark;
  entry.reason = reason;
  transactions_.runInTransaction([&] {
    submissions_.saveModeratedMark(s.id, newRawMark, hod.id());
    notifyMarkChanged(entry);
    notifyStateChanged(s.id, before, s.state);
  });
}

void ModerationService::approve(const User& user, int submissionId) {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  Assignment a;
  const Submission s = markedSubmission(hod, submissionId, a);
  // The mark is unchanged, but the log still shows who approved it and when.
  moderate(hod, s, *s.rawMark, "approved in moderation");
}

void ModerationService::adjustMark(const User& user, int submissionId, int newRawMark,
                                   const std::string& reason) {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  Assignment a;
  const Submission s = markedSubmission(hod, submissionId, a);
  if (InputValidator::isBlank(reason)) throw RuleViolation("a reason is required to adjust a mark");
  InputValidator::requireRange(newRawMark, 0, a.maxMark, "the mark");
  moderate(hod, s, newRawMark, reason);
}

int ModerationService::approveAll(const User& user, int assignmentId) {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  departmentAssignment(hod, assignmentId);
  int count = 0;
  for (const Submission& s : submissions_.findByAssignment(assignmentId)) {
    if (s.state != SubmissionState::Marked) continue;
    moderate(hod, s, *s.rawMark, "approved in moderation (bulk)");  // one entry per submission
    ++count;
  }
  return count;
}

int ModerationService::release(const User& user, int assignmentId, bool partial) {
  const HeadOfDepartment& hod = asHod(user, Action::ReleaseMarks);
  departmentAssignment(hod, assignmentId);
  std::vector<Submission> ready;
  std::string pending;
  for (const Submission& s : submissions_.findByAssignment(assignmentId)) {
    if (s.state == SubmissionState::Moderated) ready.push_back(s);
    else if (s.state != SubmissionState::Released)
      pending += (pending.empty() ? "" : ", ") + std::to_string(s.id);
  }
  if (!pending.empty() && !partial)
    throw RuleViolation("not released: these submissions are not moderated yet: " + pending);
  const TimePoint now = clock_.now();
  transactions_.runInTransaction([&] {
    for (Submission& s : ready) {
      const SubmissionState before = s.state;
      s.transitionTo(SubmissionState::Released);
      submissions_.setReleasedAt(s.id, now);
      notifyStateChanged(s.id, before, s.state);
    }
  });
  return static_cast<int>(ready.size());
}

StudentResult ModerationService::resultFor(const User& student, int assignmentId) const {
  if (!student.can(Action::ViewReleasedMarks)) throw AccessDenied("you are not allowed to do this");
  auto a = assignments_.findAssignment(assignmentId);
  if (!a) throw NotFound("assignment not found");
  if (!courses_.isEnrolled(student.id(), a->courseId)) throw AccessDenied("you are not enrolled in this course");
  StudentResult r;
  auto s = submissions_.find(assignmentId, student.id());
  if (!s) return r;
  r.submitted = true;
  r.state = s->state;
  r.released = s->state == SubmissionState::Released;
  if (r.released) r.details = buildMarkDetails(*s, *a, penalty_);  // hidden until released (BR4)
  return r;
}

std::vector<Assignment> ModerationService::departmentAssignments(const User& user) const {
  const HeadOfDepartment& hod = asHod(user, Action::ReleaseMarks);
  std::vector<Assignment> out;
  for (const Course& c : courses_.coursesInDepartment(hod.departmentId()))
    for (const Assignment& a : assignments_.findByCourse(c.id)) out.push_back(a);
  return out;
}

}  // namespace lms

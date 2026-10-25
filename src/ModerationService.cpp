#include "lms/services/ModerationService.hpp"

#include <algorithm>
#include <cctype>

namespace lms {

namespace {
bool isBlank(const std::string& s) {
  return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}
}  // namespace

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

std::vector<ModerationItem> ModerationService::awaitingModeration(const User& user) const {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  std::vector<ModerationItem> out;
  for (const Submission& s : submissions_.awaitingModeration(hod.departmentId())) {
    auto a = assignments_.findAssignment(s.assignmentId);
    if (a) out.push_back({s, *a, buildMarkDetails(s, *a, penalty_)});
  }
  return out;
}

void ModerationService::approve(const User& user, int submissionId) {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  Assignment a;
  const Submission s = markedSubmission(hod, submissionId, a);
  AuditEntry entry;
  entry.submissionId = s.id;
  entry.changedBy = hod.id();
  entry.changedAt = clock_.now();
  entry.oldValue = s.rawMark;
  entry.newValue = s.rawMark;  // unchanged: the log still shows who approved it and when
  entry.reason = "approved in moderation";
  submissions_.saveModeration(s.id, *s.rawMark, entry);
}

void ModerationService::adjust(const User& user, int submissionId, int newRawMark,
                               const std::string& reason) {
  const HeadOfDepartment& hod = asHod(user, Action::ModerateMarks);
  Assignment a;
  const Submission s = markedSubmission(hod, submissionId, a);
  if (isBlank(reason)) throw RuleViolation("a reason is required to adjust a mark");
  if (newRawMark < 0 || newRawMark > a.maxMark)
    throw RuleViolation("the mark must be between 0 and " + std::to_string(a.maxMark));
  AuditEntry entry;
  entry.submissionId = s.id;
  entry.changedBy = hod.id();
  entry.changedAt = clock_.now();
  entry.oldValue = s.rawMark;
  entry.newValue = newRawMark;
  entry.reason = reason;
  submissions_.saveModeration(s.id, newRawMark, entry);
}

int ModerationService::release(const User& user, int assignmentId) {
  const HeadOfDepartment& hod = asHod(user, Action::ReleaseMarks);
  departmentAssignment(hod, assignmentId);
  return submissions_.releaseModerated(assignmentId, clock_.now());
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

}  // namespace lms

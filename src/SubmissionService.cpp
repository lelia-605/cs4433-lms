#include "lms/services/SubmissionService.hpp"

#include <algorithm>
#include <cctype>

#include "lms/services/LateDays.hpp"

namespace lms {

namespace {
bool isBlank(const std::string& s) {
  return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}
}  // namespace

Assignment SubmissionService::studentAssignment(const User& student, int assignmentId) const {
  if (!student.can(Action::SubmitAssignment)) throw AccessDenied("you are not allowed to do this");
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  if (!courses_.isEnrolled(student.id(), assignment->courseId))
    throw AccessDenied("you are not enrolled in this course");
  return *assignment;
}

int SubmissionService::submit(const User& student, int assignmentId, const std::string& content) {
  studentAssignment(student, assignmentId);
  if (isBlank(content)) throw RuleViolation("the submission must not be empty");
  if (content.size() > kMaxContentLength)
    throw RuleViolation("the submission is too long (maximum " + std::to_string(kMaxContentLength) + ")");

  const TimePoint now = clock_.now();
  const TimePoint deadline = assignmentService_.effectiveDeadline(assignmentId, student.id());
  auto existing = submissions_.find(assignmentId, student.id());
  if (existing) {
    // A resubmission replaces the work only before the deadline and before marking (FR11).
    const bool marked = existing->state != SubmissionState::Submitted &&
                        existing->state != SubmissionState::Late;
    if (marked) throw RuleViolation("this submission has already been marked");
    if (now > deadline) throw RuleViolation("resubmission is closed: the deadline has passed");
  }

  Submission s;
  s.assignmentId = assignmentId;
  s.studentId = student.id();
  s.submittedAt = now;
  s.daysLate = daysLate(deadline, now);
  s.state = s.daysLate > 0 ? SubmissionState::Late : SubmissionState::Submitted;
  return submissions_.recordVersion(s, content);
}

std::optional<Submission> SubmissionService::mySubmission(const User& student, int assignmentId) const {
  studentAssignment(student, assignmentId);
  return submissions_.find(assignmentId, student.id());
}

std::vector<SubmissionVersion> SubmissionService::myVersions(const User& student, int assignmentId) const {
  auto s = mySubmission(student, assignmentId);
  if (!s) return {};
  return submissions_.versions(s->id);
}

std::vector<SubmissionStatus> SubmissionService::statusForAssignment(const User& lecturer,
                                                                     int assignmentId) const {
  if (!lecturer.can(Action::ViewSubmissionStatus)) throw AccessDenied("you are not allowed to do this");
  auto assignment = assignments_.findAssignment(assignmentId);
  if (!assignment) throw NotFound("assignment not found");
  auto course = courses_.findCourse(assignment->courseId);
  if (!course || course->lecturerId != lecturer.id()) throw AccessDenied("you do not teach this course");

  std::vector<SubmissionStatus> out;
  for (int studentId : courses_.enrolledStudents(assignment->courseId)) {
    SubmissionStatus row;
    row.studentId = studentId;
    row.submission = submissions_.find(assignmentId, studentId);
    row.submitted = row.submission.has_value();
    if (row.submitted) row.versionCount = static_cast<int>(submissions_.versions(row.submission->id).size());
    out.push_back(row);
  }
  return out;
}

}  // namespace lms

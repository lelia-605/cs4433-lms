#pragma once
#include "lms/data/DatabaseConnection.hpp"
#include "lms/data/SqliteAssignmentRepository.hpp"
#include "lms/data/SqliteAuditRepository.hpp"
#include "lms/data/SqliteCourseRepository.hpp"
#include "lms/data/SqliteSubmissionRepository.hpp"
#include "lms/data/SqliteUserRepository.hpp"
#include "lms/security/LibsodiumHasher.hpp"
#include "lms/services/AdminService.hpp"
#include "lms/services/AuditObserver.hpp"
#include "lms/services/AuthService.hpp"
#include "lms/services/ModerationService.hpp"
#include "lms/services/StatusObserver.hpp"
#include "lms/services/SubmissionService.hpp"

namespace lms {

// Builds every repository and service once and wires them together
// (the only place that knows which concrete classes are used). Both observers are
// attached to both services here: without them no audit entry or state is saved.
struct LmsServices {
  LmsServices(DatabaseConnection& db, const IClock& clock)
      : users(db), courses(db), assignmentsRepo(db), submissionsRepo(db), auditRepo(db),
        auditObserver(auditRepo), statusObserver(submissionsRepo),
        auth(users, hasher), admin(users, courses, hasher),
        assignments(assignmentsRepo, courses, clock),
        submissions(submissionsRepo, assignmentsRepo, courses, assignments, clock),
        marking(submissionsRepo, assignmentsRepo, courses, auditRepo, db, penalty, clock),
        moderation(submissionsRepo, assignmentsRepo, courses, db, penalty, clock) {
    marking.attach(auditObserver);
    marking.attach(statusObserver);
    moderation.attach(auditObserver);
    moderation.attach(statusObserver);
  }

  LibsodiumHasher hasher;
  StandardPenaltyStrategy penalty;
  SqliteUserRepository users;
  SqliteCourseRepository courses;
  SqliteAssignmentRepository assignmentsRepo;
  SqliteSubmissionRepository submissionsRepo;
  SqliteAuditRepository auditRepo;
  AuditObserver auditObserver;
  StatusObserver statusObserver;
  AuthService auth;
  AdminService admin;
  AssignmentService assignments;
  SubmissionService submissions;
  MarkingService marking;
  ModerationService moderation;
};

}  // namespace lms

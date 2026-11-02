# Differences between the Phase 3 design and the implementation

The code follows the Phase 3 design: layered architecture, abstract `User` with four role
subclasses, and the four patterns (Factory, Repository, Strategy, Observer). The table lists
the deliberate changes made during implementation and why.

| Design (Phase 3) | Implementation | Reason |
|---|---|---|
| `SubmissionService` submits **and** marks | Marking moved to a separate `MarkingService` | Single responsibility: submission rules (BR2, BR3) and marking rules (BR1, BR6) change for different reasons; each service stays small and is tested on its own. |
| `ISubmissionRepository::getEffectiveDeadline` | `AssignmentService::effectiveDeadline` | Extensions belong to assignments (`extension` table), so the lookup sits with the assignment repository. CR1 still needs only one place changed. |
| Audit written through `IAuditRepository` by `AuditObserver` | As designed | — |
| Observers run in the same transaction as the change | New interface `ITransactionRunner` (implemented by `DatabaseConnection`) | Lets a service wrap the save and the observer calls in one transaction without any SQL in the service. |
| `ModerationService::listForModeration`, `approve`, `adjustMark`, `release` | As designed, plus `approveAll` (UC09 A2) and `release(..., partial)` (UC09 A3, E3) | Needed for the alternative and exception flows of UC09. |
| `RoleMenus` component | Menus are methods of `ConsoleApp` (`adminMenu`, `lecturerMenu`, `hodMenu`, `studentMenu`) | Four short methods were simpler than four extra classes for a console UI. |
| — | `LmsServices` and `prepareDatabase` (new) | `LmsServices` is the single place where concrete classes are created and observers attached ("connected only in main"); `prepareDatabase` creates the schema and first administrator on first start. |
| — | `ConsoleApp` reads from any stream | Lets the system tests drive whole sessions from a script. |

FR23 and FR24 (Could) were not implemented, as planned in risk R12.

## Release v1.1 (CR1: class-wide extensions)

| Design element | Change |
|---|---|
| ERD / SQL schema | New table `class_extension` (one row per assignment: new deadline, granted by, granted at, reason). The assignment's own deadline is kept. |
| Class diagram | New struct `ClassExtension`; new enum `ExtensionKind`; `AssignmentView` gains `extensionKind`; `ModerationItem` gains `classExtension`. |
| `IAssignmentRepository` | `saveClassExtension`, `findClassExtension`. |
| `ISubmissionRepository` | `updateLateness` (new days late and state for unmarked work). |
| `AssignmentService` | `grantClassExtension`, `classDeadline`, `extensionKind`; `effectiveDeadline` now returns the latest of the assignment deadline, the class extension and the student's own extension (BR2 changed). |
| `SubmissionService` | `refreshLateness` (recalculates unmarked work after an extension); `latestVersion` (lecturer reads the work before marking, defect D14). |
| `ConsoleApp` | Lecturer option 10 "Extend the deadline for the whole class"; student and lecturer lists show the kind of extension and the original date; zero-mark warning for work more than 3 days late (defect D13). |
| `prepareDatabase` | Adds the new table to a v1.0 database on start, keeping all data. |


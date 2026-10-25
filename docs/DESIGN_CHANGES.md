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

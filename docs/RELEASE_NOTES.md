# Release notes

## v1.1 (CR1)
- Lecturers can extend the deadline for a whole class (Lecturer menu, option 10). A reason is required and the new date must be later than the current class deadline.
- Students see on their assignment list whether a deadline was extended for the whole class or for them alone, together with the original date.
- A student's deadline is the latest of the assignment deadline, the class extension and their own extension.
- When an extension is given, work that was handed in late but is not marked yet has its days late recalculated.
- Fixed: the console now warns that work more than 3 days late receives a mark of zero (D13).
- Fixed: the lecturer sees the submitted work before entering a mark (D14).
- Databases made by v1.0 are upgraded automatically on the first start; no data is lost.
- 117 automated tests (103 from v1.0 plus 14 for CR1), all passing.

## v1.0
- First release: login and four role menus; users, departments, courses and enrolment; assignments and individual extensions; submission and resubmission with versions; late penalty; marking and feedback; HoD moderation and release; append-only audit log.
- Data is kept in an SQLite file (lms.db); works with no internet connection.
- 103 automated tests, all passing; cppcheck clean; 95% line coverage of the business logic.

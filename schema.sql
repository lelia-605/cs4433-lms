PRAGMA foreign_keys = ON;

CREATE TABLE department (
  department_id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE
);

CREATE TABLE app_user (
  user_id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  full_name TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
  is_active INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
  department_id INTEGER REFERENCES department(department_id),
  CHECK (role NOT IN ('LECTURER','HOD') OR department_id IS NOT NULL)
);

CREATE TABLE course (
  course_id INTEGER PRIMARY KEY AUTOINCREMENT,
  code TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  department_id INTEGER NOT NULL REFERENCES department(department_id),
  lecturer_id INTEGER REFERENCES app_user(user_id)
);

CREATE TABLE enrolment (
  student_id INTEGER NOT NULL REFERENCES app_user(user_id),
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  PRIMARY KEY (student_id, course_id)
);

CREATE TABLE assignment (
  assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  title TEXT NOT NULL,
  deadline TEXT NOT NULL,
  max_mark INTEGER NOT NULL CHECK (max_mark > 0)
);

CREATE TABLE extension (
  extension_id INTEGER PRIMARY KEY AUTOINCREMENT,
  assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id),
  student_id INTEGER NOT NULL REFERENCES app_user(user_id),
  new_deadline TEXT NOT NULL,
  granted_by INTEGER NOT NULL REFERENCES app_user(user_id),
  granted_at TEXT NOT NULL,
  reason TEXT NOT NULL,
  UNIQUE (assignment_id, student_id)
);

CREATE TABLE submission (
  submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
  assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id),
  student_id INTEGER NOT NULL REFERENCES app_user(user_id),
  state TEXT NOT NULL CHECK (state IN ('SUBMITTED','LATE','MARKED','MODERATED','RELEASED')),
  submitted_at TEXT NOT NULL,
  days_late INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
  raw_mark INTEGER CHECK (raw_mark IS NULL OR raw_mark >= 0),
  feedback TEXT,
  marked_by INTEGER REFERENCES app_user(user_id),
  moderated_by INTEGER REFERENCES app_user(user_id),
  released_at TEXT,
  UNIQUE (assignment_id, student_id),
  CHECK (state IN ('SUBMITTED','LATE') OR raw_mark IS NOT NULL)
);

CREATE TABLE submission_version (
  version_id INTEGER PRIMARY KEY AUTOINCREMENT,
  submission_id INTEGER NOT NULL REFERENCES submission(submission_id),
  version_no INTEGER NOT NULL CHECK (version_no >= 1),
  content TEXT NOT NULL,
  submitted_at TEXT NOT NULL,
  UNIQUE (submission_id, version_no)
);

CREATE TABLE audit_log (
  audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
  submission_id INTEGER NOT NULL REFERENCES submission(submission_id),
  changed_by INTEGER NOT NULL REFERENCES app_user(user_id),
  changed_at TEXT NOT NULL,
  old_value INTEGER,
  new_value INTEGER,
  reason TEXT NOT NULL CHECK (length(trim(reason)) > 0)
);

-- one Head of Department per department
CREATE UNIQUE INDEX ux_one_hod ON app_user(department_id) WHERE role = 'HOD';
-- indexes for the main queries (NFR1, NFR2)
CREATE INDEX ix_course_department ON course(department_id);
CREATE INDEX ix_enrolment_course ON enrolment(course_id);
CREATE INDEX ix_assignment_course ON assignment(course_id);
CREATE INDEX ix_submission_student ON submission(student_id);
CREATE INDEX ix_submission_assignment ON submission(assignment_id);
CREATE INDEX ix_submission_state ON submission(state);
CREATE INDEX ix_audit_submission ON audit_log(submission_id);

-- the audit log can only be appended to (NFR11)
CREATE TRIGGER audit_no_update BEFORE UPDATE ON audit_log
BEGIN SELECT RAISE(ABORT, 'audit_log is append-only'); END;
CREATE TRIGGER audit_no_delete BEFORE DELETE ON audit_log
BEGIN SELECT RAISE(ABORT, 'audit_log is append-only'); END;

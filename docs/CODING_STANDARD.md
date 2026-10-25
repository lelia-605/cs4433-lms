# Coding standard (CS4433 Moderated LMS)

Applies to all C++ in `include/`, `src/` and `tests/`. Checked by code review, the compiler warnings and cppcheck/clang-tidy before each merge.

## Language and build
- C++17 only (`CMAKE_CXX_EXTENSIONS OFF`). Build must give **zero warnings** with `-Wall -Wextra -Wpedantic`.
- One class per header; headers use `#pragma once`; every public name is in namespace `lms`.
- No `using namespace` in headers.

## Naming
| Item | Style | Example |
|---|---|---|
| Classes, structs, enums | PascalCase | `SubmissionService` |
| Interfaces | `I` + PascalCase | `IPenaltyStrategy` |
| Functions and variables | camelCase | `findByUsername` |
| Private data members | camelCase + trailing `_` | `passwordHash_` |
| Constants | `k` + PascalCase | `kMaxLateDays` |
| Files | Same as the class | `UserFactory.hpp` |

## Layout
- 2-space indent, no tabs, maximum 100 columns. Format with `clang-format` (Google style, column limit 100).
- Braces on the same line. One statement per line.

## Safe C++
- **RAII only:** no raw `new`/`delete`; use `std::unique_ptr` / `std::make_unique`. Resources (SQLite handles, statements, transactions) are released in destructors.
- Pass by `const&` unless a copy or move is needed; mark everything that does not change as `const`.
- Base classes with virtual functions have a virtual destructor; overriding functions use `override`.
- Use `enum class`, never plain `enum`.

## Errors and validation
- Business and data code reports errors by throwing `RuleViolation`, `AccessDenied`, `NotFound` or `DatabaseError` (`lms/Errors.hpp`). No error codes, no `exit()`.
- Only the console layer catches errors and shows a message; the program continues.
- All user input is validated before it reaches a service.

## Database and security
- SQL appears **only** in classes named `Sqlite*Repository`. Values are always bound with `Statement::bind`; never concatenated into SQL.
- Passwords are handled only through `IPasswordHasher`. Plain-text passwords are never stored, logged or printed.

## Design and size
- Services depend on interfaces and receive them through the constructor.
- Functions: aim for at most 40 lines and cyclomatic complexity of at most 10.
- Comments explain *why*, not *what*. Every public interface has a one-line comment.

## Git
- Branches: `main` (always builds and passes tests), `feature/<name>`, merged by pull request. Tags `v1.0`, `v1.1`.
- Commit messages: imperative, one change each (e.g. `Add late-days calculation with boundary tests`).

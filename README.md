# Moderated LMS (CS4433)

Offline C++17 console application with SQLite storage for a secondary school. Four roles (Administrator, Lecturer, Head of Department, Student), assignments and extensions, submissions with versions, marking with the late penalty, HoD moderation and release, and an append-only audit log. Release v1.1 adds class-wide deadline extensions (CR1). 117 automated tests.

## Final report and demo
The final report, all figures and the demo video link are in [`report/`](report/README.md).

## Requirements
- A C++17 compiler (g++ 9+ or clang 10+), CMake 3.16+, pkg-config
- SQLite 3 development files and libsodium development files

Ubuntu / Debian:
```
sudo apt install build-essential cmake pkg-config libsqlite3-dev libsodium-dev
```
(Windows: install the same libraries with vcpkg and pass the vcpkg toolchain file to CMake.)

## Build, run, test
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/lms                         # runs the app with lms.db in the current folder
./build/lms school.db               # or with another database file
ctest --test-dir build --output-on-failure
```
GoogleTest is downloaded automatically during the first configure (internet needed once).

## Quality checks (as run by CI)
```
cppcheck --enable=warning,style,performance,portability --std=c++17 --language=c++ \
  --suppress=missingIncludeSystem -I include src
cmake -S . -B build-cov -DLMS_COVERAGE=ON && cmake --build build-cov --parallel
ctest --test-dir build-cov && gcovr -r . build-cov --filter src/
pip install lizard && lizard -C 10 -L 40 -w src include   # size and complexity limits
```

## First start
On the first run the app creates the database file and one administrator:
username `admin`, password `admin12345`. Log in, create the departments, users and
courses, then reset the admin password (Administrator menu, option 5).
Dates are typed as `YYYY-MM-DD HH:MM` in UTC.

## Layout
```
include/lms/   headers: domain, services, security, data, app, ui
src/           implementations and main.cpp
tests/         GoogleTest tests
schema.sql     database schema (run on first start)
docs/          coding standard
.github/workflows/ci.yml   build and test on every push
```

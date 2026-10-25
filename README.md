# Moderated LMS (CS4433)

Offline C++17 console application with SQLite storage for a secondary school. Four roles (Administrator, Lecturer, Head of Department, Student), assignments and extensions, submissions with versions, marking with the late penalty, HoD moderation and release, and an append-only audit log. 103 automated tests.

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

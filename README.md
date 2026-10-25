# Moderated LMS (CS4433)

Offline C++17 console application with SQLite storage. Walking skeleton: database layer, password hashing (Argon2id), user hierarchy and factory, login, late-penalty rule, 11 automated tests.

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
./build/lms                         # demo: logs in one user per role
ctest --test-dir build --output-on-failure
```
GoogleTest is downloaded automatically during the first configure (internet needed once).

## Layout
```
include/lms/   headers: domain, services, security, data
src/           implementations and main.cpp
tests/         GoogleTest tests
schema.sql     database schema (run on first start)
docs/          coding standard
.github/workflows/ci.yml   build and test on every push
```

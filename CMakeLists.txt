cmake_minimum_required(VERSION 3.16)
project(lms VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

# Core library: everything except main(), so the tests can use it too.
add_library(lms_core
  src/DatabaseConnection.cpp
  src/SqliteUserRepository.cpp
  src/SqliteCourseRepository.cpp
  src/AdminService.cpp
  src/Time.cpp
  src/SqliteAssignmentRepository.cpp
  src/AssignmentService.cpp
  src/SqliteSubmissionRepository.cpp
  src/SubmissionService.cpp
  src/MarkingService.cpp
  src/UserFactory.cpp
  src/LibsodiumHasher.cpp
  src/IPenaltyStrategy.cpp
  src/AuthService.cpp)
target_include_directories(lms_core PUBLIC include)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 PkgConfig::SODIUM)
target_compile_definitions(lms_core PUBLIC LMS_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/schema.sql")
target_compile_options(lms_core PUBLIC $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core)

# Tests (GoogleTest is downloaded automatically at configure time)
enable_testing()
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)

add_executable(lms_tests tests/test_penalty.cpp tests/test_users.cpp tests/test_admin.cpp tests/test_assignments.cpp
  tests/test_submissions.cpp
  tests/test_marking.cpp)
target_link_libraries(lms_tests PRIVATE lms_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(lms_tests)

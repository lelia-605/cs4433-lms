#pragma once
#include <optional>
#include <string>

namespace lms {

struct Department {
  int id = 0;
  std::string name;
};

struct Course {
  int id = 0;
  std::string code, name;
  int departmentId = 0;
  std::optional<int> lecturerId;  // empty until a lecturer is assigned
};

}  // namespace lms

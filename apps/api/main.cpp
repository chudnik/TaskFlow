#include "taskflow/domain/module.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    std::cout << "taskflow-api 0.1.0\n";
    return 0;
  }

  std::cout << "TaskFlow API scaffold (" << taskflow::domain::module_name() << ")\n";
  return 0;
}

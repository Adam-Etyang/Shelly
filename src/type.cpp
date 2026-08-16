#include "builtins.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unistd.h>

void Builtins::type(std::vector<std::string> &args) {
  if (args.size() < 2) {
    std::cout << "type: missing argument" << std::endl;
    return;
  }
  std::string arg = args[1];
  static const std::vector<std::string> builtins = {"cd",   "echo", "pwd",
                                                    "type", "jobs", "history"};
  bool is_builtin =
      std::find(builtins.begin(), builtins.end(), arg) != builtins.end();
  if (is_builtin) {
    std::cout << arg << " is a shell builtin" << std::endl;
  } else if (arg == "exit") {
    std::cout << "exit is a shell builtin" << std::endl;
  } else {
    const char *pathenv = std::getenv("PATH");
    if (pathenv) {
      std::string pathvar = pathenv;
      std::istringstream stream(pathvar);
      std::string dir;
      bool found = false;
      while (std::getline(stream, dir, ':')) {
        std::string filepath = dir + "/" + arg;
        if (std::filesystem::is_regular_file(filepath) &&
            access(filepath.c_str(), X_OK) == 0) {
          std::cout << arg << " is " << filepath << std::endl;
          found = true;
          break;
        }
      }
      if (!found) {
        std::cout << arg << ": not found" << std::endl;
      }
    }
  }
}

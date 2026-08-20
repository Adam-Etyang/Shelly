#include "builtins.hpp"
#include <iostream>

void Builtins::history(std::vector<std::string> &args) {

  for (size_t i = 0; i < prevcmds.size(); i++) {
    std::cout << i << "   " << prevcmds[i] << std::flush;
  }
}

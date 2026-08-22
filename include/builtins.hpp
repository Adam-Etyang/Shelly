#pragma once
#include <string>
#include <vector>
class Builtins {
public:
  static int cd(std::vector<std::string> &args);
  static int echo(std::vector<std::string> &args);
  static int type(std::vector<std::string> &args);
  static int pwd(std::vector<std::string> &args);
  static int history(std::vector<std::string> &args);
};

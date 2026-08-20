#pragma once
#include <string>
#include <vector>
class Builtins {
public:
  static void cd(std::vector<std::string> &args);
  static void echo(std::vector<std::string> &args);
  static void type(std::vector<std::string> &args);
  static void pwd(std::vector<std::string> &args);
  static void history(std::vector<std::string> &args);
};

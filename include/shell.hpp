#pragma once
#include "parser.hpp"
#include "process.hpp"
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using CommandFunc = std::function<void(std::vector<std::string> &)>;
class Shell {
public:
  Shell();
  void run();

private:
  Parser parser;
  Process process;
  bool prevTab = false;
  std::string lastTabLine;
  std::unordered_map<std::string, CommandFunc> commands;
  void register_builtin(const std::string &name, CommandFunc func);
  std::optional<std::string> readlineWithTab();
  void handleTab(std::string &line, bool doubleTab = false);
  int redirectout(std::vector<std::string> &args);
  int redirecterr(std::vector<std::string> &args);
};

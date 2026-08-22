#pragma once
#include "parser.hpp"
#include "process.hpp"
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using CommandFunc = std::function<int(std::vector<std::string> &)>;
class Shell {
public:
  Shell();
  void run();

private:
  Parser parser;
  Process process;
  bool prevTab = false;
  std::string lastTabLine;
  std::vector<std::string> history;
  size_t historyPos = 0;
  std::string savedLine;
  std::unordered_map<std::string, CommandFunc> commands;
  void register_builtin(const std::string &name, CommandFunc func);
  std::optional<std::string> readlineWithTab();
  void handleTab(std::string &line, bool doubleTab = false);
  void historyUp(std::string &line);
  void historyDown(std::string &line);
  void redrawLine(const std::string &line);

  void execute(const Sequence &seq);
  int executeAndOr(const AndOr &ao);
  int executePipeline(const Pipeline &pl);
  int executeCommand(const Command &cmd, bool inPipeline);
  pid_t forkAndExec(const Command &cmd, const std::vector<int> &allPipeFds, int pipeIn, int pipeOut);
};

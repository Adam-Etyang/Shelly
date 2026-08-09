#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include "parser.hpp"
#include "process.hpp"

using CommandFunc = std::function <void(std::vector<std::string>&)>;
class Shell{
  public:
    Shell();
    void run();

  private:
    Parser parser;
    Process process;
    std::unordered_map<std::string, CommandFunc> commands;
    std::vector<std::string> tokenize(std::string_view) const;
    void register_builtin(const std::string& name, CommandFunc func);

};

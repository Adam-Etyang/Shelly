#pragma once
#include <unordered_map>
#include <vector>
using CommandFunc = std::function <void(std::vector<std::string>&)>;
class Shell{
  public:
    Shell();
    void run();

  private:
    std::unordered_map<std::string, CommandFunc> commands;
    std::vector<std::string> tokenize(std::string_view) const;
    void cd(std::vector<std::string>& args);
    void echo(std::vector<std::string>& args);
    void type(std::vector<std::string>& args);
    void pwd(std::vector<std::string>& args);
    int redirect(std::vector<std::string>& args);
    bool exec(std::vector<std::string>& args);

}

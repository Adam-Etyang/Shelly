#pragma once
#include <vector>
#include <string>
class Builtins{
  public:
    static void cd(std::vector<std::string>& args);
    static void echo(std::vector<std::string>& args);
    static void type(std::vector<std::string>& args);
    static void pwd(std::vector<std::string>& args);
    static int  redirect(std::vector<std::string>& args);
};

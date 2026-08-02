#pragma once

#include<string>
#include<string_view>
#include<vector>

class Parser{
  public:
    std::vector<std::string> tokenize(std::string_view line);
};

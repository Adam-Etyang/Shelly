#pragma once

#include <string>
#include <string_view>
#include <vector>

struct Token {
  std::string text;
  bool quoted;
};

class Parser {
public:
  std::vector<Token> tokenize(std::string_view line);
};

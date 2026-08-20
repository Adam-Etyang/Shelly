#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct Token {
  std::string text;
  bool quoted;
};

struct Redirect {
  enum class Type { In, Out, Append, ErrOut, ErrAppend } type;
  std::string target;
};

struct Command {
  std::vector<std::string> arg;
  std::vector<Redirect> redirects;
};

struct Pipeline {
  std::vector<std::string> commands;
};

enum class LogicalOp { And, Or, Seq, Backgrund };

struct AndOr {
  Pipeline first;
  std::vector<std::pair<LogicalOp, Pipeline>> rest;
};

struct Sequence {
  std::vector<std::pair<AndOr, LogicalOp>> items;
};

class Parser {
  size_t pos = 0;
  std::vector<std::string> tokens;
  const Token &peek();
  const Token &advance();
  bool Check(const std::string &val);
  bool Match(const std::string &val);

public:
  std::vector<Token> tokenize(std::string_view line);

  Command ParseCommand();
  Pipeline ParsePipeline();
  AndOr ParseAndOr();
  Sequence ParseSequence();
};

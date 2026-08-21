#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Token {
  std::string text;
  bool quoted;
};

struct Redirect {
  enum class Type { In, Out, Append, ErrOut, ErrAppend, Heredoc } type;
  std::string target;
};

struct Command {
  std::vector<std::string> arg;
  std::vector<Redirect> redirects;
};

struct Pipeline {
  std::vector<Command> commands;
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
  std::vector<Token> tokens;
  Token peek();
  Token advance();
  bool Check(const std::string &val);
  bool Match(const std::string &val);

public:
  std::vector<Token> tokenize(std::string_view line);

  Command ParseCommand();
  Pipeline ParsePipeline();
  AndOr ParseAndOr();
  Sequence ParseSequence();
};

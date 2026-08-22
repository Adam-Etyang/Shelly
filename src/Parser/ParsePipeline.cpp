#include "parser.hpp"

Pipeline Parser::ParsePipeline() {
  Pipeline result;
  result.commands.push_back(ParseCommand());

  while (Match("|")) {
    if (pos >= tokens.size())
      throw ParseError("expected command after '|'");
    result.commands.push_back(ParseCommand());
  }

  return result;
}

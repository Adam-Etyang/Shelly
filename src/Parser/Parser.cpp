#include "parser.hpp"

Sequence Parser::parse(std::string_view line) {
  tokenize(line);
  if (tokens.empty())
    return {};

  Sequence result = ParseSequence();
  if (pos < tokens.size())
    throw ParseError("unexpected '" + peek().text + "'");
  return result;
}

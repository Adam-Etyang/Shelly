#include "parser.hpp"

/*
 * struct AndOr {
 *   Pipeline first;
 *   std::vector<std::pair<LogicalOp, Pipeline>> rest;
 * };
 *
 * given the line "cmd1 && cmd2 || cmd3"
 * first = [cmd1]
 * rest = [(And, [cmd2]), (Or, [cmd3])]
 * */

AndOr Parser::ParseAndOr() {
  AndOr result;
  result.first = ParsePipeline();

  while (pos < tokens.size()) {
    const Token &current = peek();
    if (current.quoted ||
        (current.text != "&&" && current.text != "||"))
      break;

    LogicalOp op = (current.text == "&&") ? LogicalOp::And : LogicalOp::Or;
    advance();

    if (pos >= tokens.size())
      throw ParseError("expected command after '" + current.text + "'");

    result.rest.emplace_back(op, ParsePipeline());
  }

  return result;
}

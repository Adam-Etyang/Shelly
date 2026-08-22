#include "parser.hpp"

/*
 * struct Sequence {
 *   std::vector<std::pair<AndOr, LogicalOp>> items;
 * };
 *
 * given the line "cmd1 ; cmd2 & cmd3"
 * items = [(andOr1, Seq), (andOr2, Background), (andOr3, Seq)]
 * the op is the separator that follows each AndOr
 * a missing trailing separator defaults to Seq
 */

Sequence Parser::ParseSequence() {
  Sequence result;

  while (true) {
    if (pos < tokens.size() && !tokens[pos].quoted &&
        (tokens[pos].text == ";" || tokens[pos].text == "&"))
      throw ParseError("unexpected '" + tokens[pos].text + "'");

    AndOr item = ParseAndOr();

    if (Match(";")) {
      result.items.emplace_back(std::move(item), LogicalOp::Seq);
    } else if (Match("&")) {
      result.items.emplace_back(std::move(item), LogicalOp::Background);
    } else {
      result.items.emplace_back(std::move(item), LogicalOp::Seq);
      break;
    }

    if (pos >= tokens.size())
      break;
  }

  return result;
}

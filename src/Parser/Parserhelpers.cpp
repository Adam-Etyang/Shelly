#include "parser.hpp"
#include <cassert>

// returns the token at current pos
Token Parser::peek() {
  assert(pos < tokens.size());
  return tokens[pos];
}

// returns token that was consumed
Token Parser::advance() {
  assert(pos < tokens.size());
  Token current = tokens[pos];
  pos++;
  return current;
}

/*Returns true if not at end,
the current token (peek()) is unquoted,
and its .text equals text.
*/
bool Parser::Check(const std::string &val) {
  return pos < tokens.size() && !tokens[pos].quoted && tokens[pos].text == val);
}

/*Calls check(text);
if true, calls advance() and returns true;
if false, does nothing and returns false.
*/
bool Parser::Match(const std::string &val) {
  if (Check(val)) {
    advance();
    return true;
  }
  return false;
}

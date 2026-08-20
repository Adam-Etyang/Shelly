#include "parser.hpp"

// retutns the token at current pos
Token Parser::peek() {}

// returns token that was consumed
Token Parser::advance() { return }

/*Returns true if not at end,
the current token (peek()) is unquoted,
and its .text equals text.
*/
bool Parser::Check(const std::string &val) {}

/*Calls check(text);
if true, calls advance() and returns true;
if false, does nothing and returns false.
*/
bool Parser::Match(const std::string &val) {}

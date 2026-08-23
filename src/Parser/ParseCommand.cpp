#include "parser.hpp"

namespace {

bool isControlOp(const std::string &text) {
  return text == "|" || text == "&" || text == ";" || text == ")" ||
         text == "&&" || text == "||";
}

bool isRedirectOp(const std::string &text) {
  return text == ">" || text == ">>" || text == "<" || text == "<<" ||
         text == "1>" || text == "2>" || text == "1>>" || text == "2>>";
}

Redirect::Type redirectType(const std::string &op) {
  if (op == ">" || op == "1>")
    return Redirect::Type::Out;
  if (op == ">>" || op == "1>>")
    return Redirect::Type::Append;
  if (op == "<")
    return Redirect::Type::In;
  if (op == "2>")
    return Redirect::Type::ErrOut;
  if (op == "2>>")
    return Redirect::Type::ErrAppend;
  if (op == "<<")
    return Redirect::Type::Heredoc;
  throw ParseError("unknown redirect operator '" + op + "'");
}

} // namespace

Command Parser::ParseCommand() {
  std::vector<std::string> args;
  std::vector<Redirect> redirects;

  if (pos < tokens.size()) {
    const Token &first = peek();
    if (!first.quoted && isControlOp(first.text))
      throw ParseError("unexpected '" + first.text + "'");
  }

  while (pos < tokens.size()) {
    Token current = peek();

    if (!current.quoted && isControlOp(current.text))
      break;

    if (!current.quoted && isRedirectOp(current.text)) {
      advance();

      if (pos >= tokens.size())
        throw ParseError("expected file name after '" + current.text + "'");

      Token target = advance();
      if (!target.quoted &&
          (isControlOp(target.text) || isRedirectOp(target.text)))
        throw ParseError("expected file name after '" + current.text +
                         "', got '" + target.text + "'");

      redirects.push_back(Redirect{redirectType(current.text), target.text});

    } else {
      args.push_back(current.text);
      advance();
    }
  }

  return {args, redirects};
}

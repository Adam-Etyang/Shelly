#include "parser.hpp"
#include <cctype>

void Parser::tokenize(std::string_view line) {
  bool singleQuote = false;
  bool doubleQuote = false;
  bool tokenQuote = false;
  bool tokenStarted = false;
  std::string current;
  std::vector<Token> args;

  auto flush = [&]() {
    if (tokenStarted) {
      args.push_back(Token{current, tokenQuote});
      current.clear();
      tokenQuote = false;
      tokenStarted = false;
    }
  };

  for (size_t i = 0; i < line.size(); i++) {
    char c = line[i];

    if (singleQuote) {
      if (c == '\'') {
        singleQuote = false;
      }

      else {
        current += c;
        tokenStarted = true;
        tokenQuote = true;
      }

    }

    else if (doubleQuote) {
      if (c == '"') {
        doubleQuote = false;
      }

      else if (c == '\\' && i + 1 < line.size() &&
               (line[i + 1] == '\\' || line[i + 1] == '"' ||
                line[i + 1] == '$')) {

        current += line[i + 1];
        tokenStarted = true;
        tokenQuote = true;
        i++;
      }

      else {
        current += c;
        tokenStarted = true;
        tokenQuote = true;
      }
    } else {
      if (c == '\'') {
        singleQuote = true;
        tokenStarted = true;
        tokenQuote = true;
      }

      else if (c == '"') {
        doubleQuote = true;
        tokenStarted = true;
        tokenQuote = true;
      }

      else if (c == '\\' && i + 1 < line.size()) {
        current += line[i + 1];
        tokenStarted = true;
        i++;
      }

      else if (std::isspace(static_cast<unsigned char>(c))) {
        flush();
      }

      else if (c == '|' || c == '<' || c == '>' || c == '&' || c == ';' ||
               c == '(' || c == ')') {

        if (c == '>' && !tokenQuote && (current == "1" || current == "2")) {
          std::string fdTok = current;
          current.clear();
          tokenQuote = false;
          tokenStarted = false;
          if (i + 1 < line.size() && line[i + 1] == '>') {
            fdTok += ">>";
            i++;
          }

          else {
            fdTok += '>';
          }
          args.push_back(Token{fdTok, false});
          continue;
        }

        flush();

        if (i + 1 < line.size() && ((c == '>' && line[i + 1] == '>') ||
                                    (c == '<' && line[i + 1] == '<') ||
                                    (c == '&' && line[i + 1] == '&') ||
                                    (c == '|' && line[i + 1] == '|'))) {
          args.push_back(Token{std::string(1, c) + line[i + 1], false});
          i++;
        }

        else {
          args.push_back(Token{std::string(1, c), false});
        }
      }

      else {
        current += c;
        tokenStarted = true;
      }
    }
  }
  flush();
  tokens = std::move(args);
  pos = 0;
}

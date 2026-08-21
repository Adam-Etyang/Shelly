#include "parser.hpp"
#include <cctype>

std::vector<Token> Parser::tokenize(std::string_view line) {
  bool singleQuote = false;
  bool doubleQuote = false;
  bool tokenQuote = false;
  std::string current;
  std::vector<Token> args;

  auto flush = [&]() {
    if (!current.empty()) {
      args.push_back(Token{current, tokenQuote});
      current.clear();
      tokenQuote = false;
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
        tokenQuote = true;
        i++;
      }

      else {
        current += c;
        tokenQuote = true;
      }
    } else {
      if (c == '\'') {
        singleQuote = true;
      }

      else if (c == '"') {
        doubleQuote = true;
      }

      else if (c == '\\' && i + 1 < line.size()) {
        current += line[i + 1];
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
      }
    }
  }
  flush();
  tokens = std::move(args);
  pos = 0;
  return tokens;
}

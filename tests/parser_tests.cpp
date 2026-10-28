#include "parser.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void testQuotedTokens() {
  Parser parser;
  const auto tokens = parser.tokenize("echo \"two words\" 'single' \"\"");

  check(tokens.size() == 4, "quoted input preserves each argument");
  if (tokens.size() == 4) {
    check(tokens[0].text == "echo" && !tokens[0].quoted,
          "plain command name is unquoted");
    check(tokens[1].text == "two words" && tokens[1].quoted,
          "double-quoted argument remains one quoted token");
    check(tokens[2].text == "single" && tokens[2].quoted,
          "single-quoted argument remains one quoted token");
    check(tokens[3].text.empty() && tokens[3].quoted,
          "empty quoted argument is preserved");
  }
}

void testAndOrPipelineAndSequenceParsing() {
  Parser parser;
  parser.tokenize(
      "echo \"hello world\" | cat && false || echo recovered; echo later &");
  const Sequence sequence = parser.ParseSequence();

  check(sequence.items.size() == 2, "sequence has two entries");
  if (sequence.items.size() != 2)
    return;

  const AndOr &first = sequence.items[0].first;
  check(sequence.items[0].second == LogicalOp::Seq,
        "semicolon marks the first sequence entry");
  check(first.first.commands.size() == 2,
        "pipeline contains both commands");
  if (first.first.commands.size() == 2) {
    check(first.first.commands[0].arg.size() == 2 &&
              first.first.commands[0].arg[1] == "hello world",
          "quoted argument survives parsing inside a pipeline");
    check(first.first.commands[1].arg.size() == 1 &&
              first.first.commands[1].arg[0] == "cat",
          "second pipeline command is parsed");
  }
  check(first.rest.size() == 2, "AND/OR chain contains two operators");
  if (first.rest.size() == 2) {
    check(first.rest[0].first == LogicalOp::And,
          "first logical operator is AND");
    check(first.rest[1].first == LogicalOp::Or,
          "second logical operator is OR");
  }

  check(sequence.items[1].second == LogicalOp::Background,
        "ampersand marks the final entry as background");
  const auto &lastCommands = sequence.items[1].first.first.commands;
  check(lastCommands.size() == 1 && !lastCommands[0].arg.empty() &&
            lastCommands[0].arg[0] == "echo",
        "command after semicolon is parsed");
}

void testRedirectionParsing() {
  Parser parser;
  parser.tokenize("cat < input > output >> append 2> errors 2>> more << END");
  const Sequence sequence = parser.ParseSequence();
  check(sequence.items.size() == 1, "redirection line is one sequence entry");
  if (sequence.items.size() != 1)
    return;

  const auto &commands = sequence.items[0].first.first.commands;
  check(commands.size() == 1, "redirection line is one command");
  if (commands.size() != 1)
    return;
  const Command &command = commands[0];

  const std::vector<Redirect::Type> expectedTypes = {
      Redirect::Type::In,       Redirect::Type::Out,
      Redirect::Type::Append,   Redirect::Type::ErrOut,
      Redirect::Type::ErrAppend, Redirect::Type::Heredoc};
  const std::vector<std::string> expectedTargets = {
      "input", "output", "append", "errors", "more", "END"};

  check(command.arg.size() == 1 && command.arg[0] == "cat",
        "redirection targets are not treated as command arguments");
  check(command.redirects.size() == expectedTypes.size(),
        "all supported redirection operators are parsed");
  if (command.redirects.size() == expectedTypes.size()) {
    for (size_t i = 0; i < expectedTypes.size(); ++i) {
      check(command.redirects[i].type == expectedTypes[i],
            "redirection type " + std::to_string(i) + " is correct");
      check(command.redirects[i].target == expectedTargets[i],
            "redirection target " + std::to_string(i) + " is correct");
    }
  }
}

void testQuotedOperatorsAreArguments() {
  Parser parser;
  parser.tokenize("echo '|' \"&&\"");
  const Sequence sequence = parser.ParseSequence();

  check(sequence.items.size() == 1, "quoted operators do not split sequences");
  if (sequence.items.size() != 1)
    return;

  const auto &commands = sequence.items[0].first.first.commands;
  check(commands.size() == 1,
        "quoted pipe does not create a pipeline");
  if (commands.size() != 1)
    return;

  const Command &command = commands[0];
  check(command.arg.size() == 3 && command.arg[1] == "|" &&
            command.arg[2] == "&&",
        "quoted control operators remain ordinary arguments");
}

void testIncompleteExpressionsAreRejected() {
  for (const std::string input : {"echo |", "echo &&", "echo ||"}) {
    Parser parser;
    parser.tokenize(input);
    bool threw = false;
    try {
      (void)parser.ParseSequence();
    } catch (const ParseError &) {
      threw = true;
    }
    check(threw, "incomplete expression is rejected: " + input);
  }
}

} // namespace

int main() {
  testQuotedTokens();
  testAndOrPipelineAndSequenceParsing();
  testRedirectionParsing();
  testQuotedOperatorsAreArguments();
  testIncompleteExpressionsAreRejected();

  if (failures != 0) {
    std::cerr << failures << " parser test(s) failed\n";
    return 1;
  }

  std::cout << "All parser tests passed\n";
  return 0;
}

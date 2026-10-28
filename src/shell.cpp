#include "shell.hpp"
#include "builtins.hpp"

#include <cctype>
#include <csignal>
#include <iostream>
#include <string_view>
#include <termios.h>
#include <unistd.h>

namespace {

termios originalTermios;

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &originalTermios);
  termios raw = originalTermios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
}

bool isExitCommand(std::string_view line) {
  size_t start = 0;
  while (start < line.size() &&
         std::isspace(static_cast<unsigned char>(line[start]))) {
    ++start;
  }

  if (line.substr(start, 4) != "exit")
    return false;
  if (start + 4 == line.size())
    return true;

  const char next = line[start + 4];
  return std::isspace(static_cast<unsigned char>(next)) ||
         std::string_view(";&|<>()").find(next) != std::string_view::npos;
}

} // namespace

Shell::Shell() {
  // Prevent shell itself from dying on Ctrl-C / Ctrl-\ / Ctrl-Z
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  commands["cd"] = Builtins::cd;
  commands["echo"] = Builtins::echo;
  commands["pwd"] = Builtins::pwd;
  commands["type"] = Builtins::type;
  commands["fg"] = [this](std::vector<std::string> &args) {
    std::string spec = args.size() > 1 ? args[1] : "";
    return process.fg(spec) ? 0 : 1;
  };
  commands["bg"] = [this](std::vector<std::string> &args) {
    std::string spec = args.size() > 1 ? args[1] : "";
    return process.bg(spec) ? 0 : 1;
  };
  commands["jobs"] = [this](std::vector<std::string> &) {
    process.printJobs();
    return 0;
  };
  commands["history"] = [this](std::vector<std::string> &) {
    for (size_t i = 0; i < history.size(); i++) {
      std::cout << i + 1 << "  " << history[i] << std::endl;
    }
    return 0;
  };
}

void Shell::run() {
  enableRawMode();
  prevTab = false;
  while (true) {
    process.reapJobs();
    std::cout << "$ " << std::flush;
    auto maybeline = readlineWithTab();
    if (!maybeline.has_value()) {
      break;
    }
    std::string line = maybeline.value();

    if (isExitCommand(line))
      break;

    try {
      Sequence seq = parser.parse(line);
      if (seq.items.empty())
        continue;

      if (history.empty() || history.back() != line) {
        history.push_back(line);
      }

      disableRawMode();
      execute(seq);
      enableRawMode();
    } catch (const std::exception &e) {
      disableRawMode();
      std::cerr << e.what() << std::endl;
      enableRawMode();
    }
  }
  disableRawMode();
}

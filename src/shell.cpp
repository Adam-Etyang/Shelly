#include "shell.hpp"
#include "builtins.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <sys/_types/_pid_t.h>
#include <sys/wait.h>
#include <system_error>
#include <termios.h>
#include <unistd.h>
#include <vector>

termios orig_termios;
void enableRawmode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  termios raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawmode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

Shell::Shell() {
  commands["cd"] = Builtins::cd;
  commands["echo"] = Builtins::echo;
  commands["pwd"] = Builtins::pwd;
  commands["type"] = Builtins::type;
  commands["jobs"] = [this](std::vector<std::string> &) {
    process.printJobs();
  };
  commands["history"] = [this](std::vector<std::string> &) {
    for (size_t i = 0; i < history.size(); i++) {
      std::cout << i + 1 << "  " << history[i] << std::endl;
    }
  };
}

void Shell::run() {
  enableRawmode();
  prevTab = false;
  while (true) {
    process.reapJobs();
    std::cout << "$ " << std::flush;
    auto maybeline = readlineWithTab();
    if (!maybeline.has_value()) {
      break;
    }
    std::string line = maybeline.value();

    auto tokens = parser.tokenize(line);
    if (tokens.empty())
      continue;

    std::vector<std::string> args;
    args.reserve(tokens.size());
    for (const auto &t : tokens) {
      args.push_back(t.text);
    }

    if (args[0] == "exit") {
      break;
    }

    if (history.empty() || history.back() != line) {
      history.push_back(line);
    }

    try {
      int saved_err = redirecterr(args);
      int saved_out = redirectout(args);

      if (std::find(args.begin(), args.end(), "|") != args.end()) {
        disableRawmode();
        pipeline(args);
        enableRawmode();
      } else {
        auto it = commands.find(args[0]);

        if (it != commands.end()) {
          args.erase(std::remove(args.begin(), args.end(), "&"), args.end());
          it->second(args);
        } else {
          disableRawmode();
          process.exec(args);
          enableRawmode();
        }
      }

      if (saved_out != -1) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
      }
      if (saved_err != -1) {
        dup2(saved_err, STDERR_FILENO);
        close(saved_err);
      }
    } catch (const std::exception &e) {
      disableRawmode();
      std::cerr << e.what() << std::endl;
      enableRawmode();
    }
  }
  disableRawmode();
}

void Shell::register_builtin(const std::string &name, CommandFunc func) {
  commands[name] = func;
}

void Shell::handleTab(std::string &line, bool doubleTab) {
  std::vector<std::string> matches;
  size_t lastSpace = line.find_last_of(' ');
  bool isFirstword = (lastSpace == std::string::npos);
  std::string currentWord =
      (lastSpace == std::string::npos) ? line : line.substr(lastSpace + 1);

  // build candidate list
  if (isFirstword) {
    for (auto &pair : commands) {
      if (pair.first.starts_with(currentWord)) {
        matches.push_back(pair.first);
      }
    }

    // find executables in PATH
    const char *pathenv = std::getenv("PATH");
    if (pathenv) {
      std::string pathdir = pathenv;
      std::string dir;
      std::istringstream stream(pathdir);
      while (std::getline(stream, dir, ':')) {
        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec);
        if (ec) {
          continue;
        }

        for (const auto &entry : it) {
          std::string filepath = entry.path();
          std::string filename = entry.path().filename().string();
          if (filename.rfind(currentWord, 0) == 0) {
            if (entry.is_regular_file() &&
                access(filepath.c_str(), X_OK) == 0) {
              if (std::find(matches.begin(), matches.end(), filename) ==
                  matches.end()) {
                matches.push_back(filename);
              }
            }
          }
        }
      }
    }
  } else {
    // gather candidates for file completions
    // 1. Get dir to search
    size_t slash_pos = currentWord.find_last_of('/');
    std::string path = (slash_pos != std::string::npos)
                           ? currentWord.substr(0, slash_pos + 1)
                           : ".";
    std::string prefix = (slash_pos != std::string::npos)
                             ? currentWord.substr(slash_pos + 1)
                             : currentWord;
    std::error_code ec;
    std::filesystem::directory_iterator it(path, ec);
    if (ec) {
      return;
    }
    for (const auto &entry : it) {
      std::string filename = entry.path().filename().string();
      if (filename.rfind(prefix, 0) == 0) {
        std::string candidate = (path == ".") ? filename : path + filename;
        if (std::find(matches.begin(), matches.end(), candidate) ==
            matches.end()) {
          matches.push_back(candidate);
        }
      }
    }
  }

  // completion logic
  std::sort(matches.begin(), matches.end());

  if (matches.empty()) {
    std::cout << "\x07" << std::flush;
    return;
  }

  std::string lcp = matches[0];

  for (size_t i = 1; i < matches.size(); ++i) {
    size_t j = 0;
    while (j < lcp.size() && j < matches[i].size() && lcp[j] == matches[i][j]) {
      ++j;
    }
    lcp.resize(j);
  }

  if (lcp.size() > currentWord.size()) {
    std::string cmp = lcp.substr(currentWord.size());
    line += cmp;
    std::cout << cmp << std::flush;
    return;
  }

  if (matches.size() == 1) {
    std::string cmp = matches[0].substr(currentWord.size());
    line += cmp;
    line += ' ';
    std::cout << cmp << ' ' << std::flush;
    return;
  }

  if (doubleTab) {
    std::cout << "\r\n";
    for (size_t i = 0; i < matches.size(); ++i) {
      std::cout << matches[i];
      if (i + 1 < matches.size()) {
        std::cout << "  ";
      }
    }
    std::cout << "\r\n$ " << line << std::flush;
  } else {
    std::cout << "\x07" << std::flush;
  }
}

std::optional<std::string> Shell::readlineWithTab() {
  std::string line;
  char c;
  historyPos = history.size();

  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      prevTab = false;
      std::cout << std::endl;
      return line;
    } else if (c == '\x1b') {
      char seq[2];
      if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
          read(STDIN_FILENO, &seq[1], 1) != 1) {
        continue;
      }
      if (seq[0] == '[' && seq[1] == 'A') {
        historyUp(line);
      } else if (seq[0] == '[' && seq[1] == 'B') {
        historyDown(line);
      }
    } else if (c == '\t') {
      bool doubleTab = prevTab && (line == lastTabLine);
      handleTab(line, doubleTab);
      prevTab = true;
      lastTabLine = line;
    } else if (c == '\x7f' || c == '\x08') {
      prevTab = false;
      if (!line.empty()) {
        line.pop_back();
        std::cout << "\b \b" << std::flush;
      }
    } else {
      prevTab = false;
      line += c;
      std::cout << c << std::flush;
    }
  }
  return std::nullopt;
}

void Shell::historyUp(std::string &line) {
  if (history.empty())
    return;
  prevTab = false;
  if (historyPos == history.size()) {
    savedLine = line;
    historyPos = history.size() - 1;
  } else if (historyPos > 0) {
    historyPos--;
  } else {
    return;
  }
  line = history[historyPos];
  redrawLine(line);
}

void Shell::historyDown(std::string &line) {
  if (historyPos >= history.size())
    return;
  prevTab = false;
  historyPos++;
  line = (historyPos == history.size()) ? savedLine : history[historyPos];
  redrawLine(line);
}

void Shell::redrawLine(const std::string &line) {
  std::cout << "\r\x1b[K$ " << line << std::flush;
}

int Shell::redirectout(std::vector<std::string> &args) {
  auto redirect_it =
      std::find_if(args.begin(), args.end(),
                   [](const std::string &s) { return s == ">" || s == ">>"; });

  if (redirect_it == args.end()) {
    redirect_it =
        std::find_if(args.begin(), args.end(), [](const std::string &s) {
          return s == "1>" || s == "1>>";
        });
  }

  if (redirect_it == args.end()) {
    return -1;
  }

  bool append = (*redirect_it == ">>" || *redirect_it == "1>>");

  std::vector<std::string> cmd_args(args.begin(), redirect_it);
  std::vector<std::string> out_files(redirect_it + 1, args.end());

  if (!out_files.empty()) {
    int file_fd =
        open(out_files[0].c_str(),
             O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);

    if (file_fd < 0) {
      perror("open");
      return -1;
    }
    int save_out = dup(STDOUT_FILENO);
    dup2(file_fd, STDOUT_FILENO);

    close(file_fd);
    args = std::move(cmd_args);

    return save_out;
  }
  return -1;
}

int Shell::redirecterr(std::vector<std::string> &args) {
  auto redirect_it =
      std::find_if(args.begin(), args.end(), [](const std::string &s) {
        return s == "2>" || s == "2>>";
      });
  if (redirect_it == args.end()) {
    return -1;
  }

  bool append = (*redirect_it == "2>>");

  std::vector<std::string> cmd_args(args.begin(), redirect_it);
  std::vector<std::string> out_files(redirect_it + 1, args.end());
  if (!out_files.empty()) {
    int file_fd =
        open(out_files[0].c_str(),
             O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
    if (file_fd < 0) {
      perror("open");
      return -1;
    }
    int save_out = dup(STDERR_FILENO);
    dup2(file_fd, STDERR_FILENO);
    args = std::move(cmd_args);
    close(file_fd);
    return save_out;
  }
  return -1;
}

int Shell::pipeline(std::vector<std::string> &args) {
  std::vector<std::vector<std::string>> cmds;
  std::vector<std::string> currentcmd;
  for (const auto &arg : args) {
    if (arg == "|") {
      if (!currentcmd.empty()) {
        cmds.push_back(currentcmd);
        currentcmd.clear();
      }
    } else {
      currentcmd.push_back(arg);
    }
  }
  if (!currentcmd.empty()) {
    cmds.push_back(currentcmd);
  }
  if (cmds.size() < 2) {
    return -1;
  }

  size_t numpipes = cmds.size() - 1;
  std::vector<int> pipefds(2 * numpipes);

  for (size_t i = 0; i < numpipes; i++) {
    if (pipe(pipefds.data() + i * 2) == -1) {
      perror("pipe");
      return -1;
    }
  }

  std::vector<pid_t> pids;
  pids.reserve(cmds.size());

  for (size_t i = 0; i < cmds.size(); i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return -1;
    }
    if (pid == 0) {
      if (i > 0) {
        int readfd = pipefds[(i - 1) * 2];
        dup2(readfd, STDIN_FILENO);
        close(readfd);
      }

      if (i < cmds.size() - 1) {
        int writefd = pipefds[i * 2 + 1];
        dup2(writefd, STDOUT_FILENO);
        close(writefd);
      }

      for (int fd : pipefds) {
        close(fd);
      }

      std::vector<char *> argv;
      for (auto &arg : cmds[i]) {
        argv.push_back(const_cast<char *>(arg.c_str()));
      }
      argv.push_back(nullptr);
      execvp(argv[0], argv.data());

      if (errno == ENOENT) {
        std::cerr << argv[0] << ": command not found" << std::endl;
      } else {
        perror("execvp");
      }
      _exit(EXIT_FAILURE);
    }
    pids.push_back(pid);
  }

  for (int fd : pipefds) {
    close(fd);
  }

  for (size_t i = 0; i < pids.size(); i++) {
    int status;
    waitpid(pids[i], &status, 0);
  }

  return 0;
}

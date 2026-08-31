#include "shell.hpp"
#include "builtins.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <sstream>
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

// ─── Redirect helpers ───────────────────────────────────────────────

static bool applyRedirects(const std::vector<Redirect> &redirects, int &savedIn,
                           int &savedOut, int &savedErr) {
  savedIn = savedOut = savedErr = -1;
  for (const auto &r : redirects) {
    int fd = -1;
    int target = -1;
    switch (r.type) {
    case Redirect::Type::In:
      fd = open(r.target.c_str(), O_RDONLY);
      target = STDIN_FILENO;
      break;
    case Redirect::Type::Out:
      fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      target = STDOUT_FILENO;
      break;
    case Redirect::Type::Append:
      fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      target = STDOUT_FILENO;
      break;
    case Redirect::Type::ErrOut:
      fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      target = STDERR_FILENO;
      break;
    case Redirect::Type::ErrAppend:
      fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      target = STDERR_FILENO;
      break;
    case Redirect::Type::Heredoc: {
      std::string content, line;
      while (std::getline(std::cin, line)) {
        if (line == r.target)
          break;
        content += line;
        content += '\n';
      }
      FILE *tmp = tmpfile();
      if (!tmp)
        return false;
      fwrite(content.data(), 1, content.size(), tmp);
      fflush(tmp);
      rewind(tmp);
      fd = dup(fileno(tmp));
      fclose(tmp);
      target = STDIN_FILENO;
      break;
    }
    }
    if (fd < 0)
      return false;
    if (target == STDIN_FILENO && savedIn == -1)
      savedIn = dup(STDIN_FILENO);
    else if (target == STDOUT_FILENO && savedOut == -1)
      savedOut = dup(STDOUT_FILENO);
    else if (target == STDERR_FILENO && savedErr == -1)
      savedErr = dup(STDERR_FILENO);
    dup2(fd, target);
    close(fd);
  }
  return true;
}

static void restoreRedirects(int savedIn, int savedOut, int savedErr) {
  if (savedIn != -1) {
    dup2(savedIn, STDIN_FILENO);
    close(savedIn);
  }
  if (savedOut != -1) {
    dup2(savedOut, STDOUT_FILENO);
    close(savedOut);
  }
  if (savedErr != -1) {
    dup2(savedErr, STDERR_FILENO);
    close(savedErr);
  }
}

// ─── AST executor ───────────────────────────────────────────────────

static std::string describePipeline(const Pipeline &pl) {
  std::string out;
  for (size_t i = 0; i < pl.commands.size(); i++) {
    if (i)
      out += " | ";
    for (size_t j = 0; j < pl.commands[i].arg.size(); j++) {
      if (j)
        out += ' ';
      out += pl.commands[i].arg[j];
    }
  }
  return out;
}

static std::string describeAndOr(const AndOr &ao) {
  std::string out = describePipeline(ao.first);
  for (auto &[op, pl] : ao.rest)
    out += (op == LogicalOp::And ? " && " : " || ") + describePipeline(pl);
  return out;
}

pid_t Shell::forkAndExec(const Command &cmd, const std::vector<int> &allPipeFds,
                         int pipeIn, int pipeOut) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    if (pipeIn != -1)
      dup2(pipeIn, STDIN_FILENO);
    if (pipeOut != -1)
      dup2(pipeOut, STDOUT_FILENO);

    for (int fd : allPipeFds)
      close(fd);

    int si = -1, so = -1, se = -1;
    if (!applyRedirects(cmd.redirects, si, so, se))
      _exit(1);

    auto it = commands.find(cmd.arg[0]);
    if (it != commands.end()) {
      std::vector<std::string> args = cmd.arg;
      _exit(it->second(args));
    }

    std::vector<char *> argv;
    for (auto &s : cmd.arg)
      argv.push_back(const_cast<char *>(s.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    if (errno == ENOENT)
      std::cerr << cmd.arg[0] << ": command not found" << std::endl;
    else
      perror("execvp");
    _exit(127);
  }

  return pid;
}

int Shell::executeCommand(const Command &cmd, bool inPipeline) {
  if (cmd.arg.empty())
    return 0;

  auto it = commands.find(cmd.arg[0]);
  bool isBuiltin = (it != commands.end());

  if (inPipeline || !isBuiltin) {
    std::vector<int> empty;
    pid_t pid = forkAndExec(cmd, empty, -1, -1);
    if (pid < 0)
      return 1;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }

  std::vector<std::string> args = cmd.arg;

  if (cmd.redirects.empty()) {
    return it->second(args);
  }

  int si = -1, so = -1, se = -1;
  if (!applyRedirects(cmd.redirects, si, so, se)) {
    std::cerr << args[0] << ": redirect failed" << std::endl;
    return 1;
  }
  int code = it->second(args);
  restoreRedirects(si, so, se);
  return code;
}

int Shell::executePipeline(const Pipeline &pl) {
  if (pl.commands.empty())
    return 0;

  if (pl.commands.size() == 1)
    return executeCommand(pl.commands[0], false);

  size_t n = pl.commands.size();
  std::vector<int> pipeFds(2 * (n - 1));
  for (size_t i = 0; i < n - 1; i++) {
    if (pipe(pipeFds.data() + i * 2) < 0) {
      perror("pipe");
      return 1;
    }
  }

  std::vector<pid_t> pids;
  pids.reserve(n);
  for (size_t i = 0; i < n; i++) {
    int pipeIn = (i > 0) ? pipeFds[(i - 1) * 2] : -1;
    int pipeOut = (i < n - 1) ? pipeFds[i * 2 + 1] : -1;
    pid_t pid = forkAndExec(pl.commands[i], pipeFds, pipeIn, pipeOut);
    if (pid < 0) {
      for (int fd : pipeFds)
        close(fd);
      return 1;
    }
    pids.push_back(pid);
  }

  for (int fd : pipeFds)
    close(fd);

  int status = 0;
  for (size_t i = 0; i < pids.size(); i++) {
    int st = 0;
    waitpid(pids[i], &st, 0);
    if (i == pids.size() - 1 && WIFEXITED(st))
      status = WEXITSTATUS(st);
  }
  return status;
}

int Shell::executeAndOr(const AndOr &ao) {
  int status = executePipeline(ao.first);
  for (auto &[op, pl] : ao.rest) {
    if (op == LogicalOp::And && status != 0)
      continue;
    if (op == LogicalOp::Or && status == 0)
      continue;
    status = executePipeline(pl);
  }
  return status;
}

void Shell::execute(const Sequence &seq) {
  for (auto &[andOr, op] : seq.items) {
    if (op == LogicalOp::Background) {
      pid_t pid = fork();
      if (pid < 0) {
        perror("fork");
        continue;
      }
      if (pid == 0) {
        int s = executeAndOr(andOr);
        _exit(s);
      }
      process.addBackgroundJob(pid, describeAndOr(andOr));
    } else {
      executeAndOr(andOr);
    }
  }
}

// ─── Shell loop ─────────────────────────────────────────────────────

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

    try {
      auto tokens = parser.tokenize(line);
      if (tokens.empty())
        continue;

      if (!tokens[0].quoted && tokens[0].text == "exit") {
        break;
      }

      if (history.empty() || history.back() != line) {
        history.push_back(line);
      }

      Sequence seq = parser.ParseSequence();

      disableRawmode();
      execute(seq);
      enableRawmode();
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

// ─── Tab completion ─────────────────────────────────────────────────

void Shell::handleTab(std::string &line, bool doubleTab) {
  std::vector<std::string> matches;
  size_t lastSpace = line.find_last_of(' ');
  bool isFirstword = (lastSpace == std::string::npos);
  std::string currentWord =
      (lastSpace == std::string::npos) ? line : line.substr(lastSpace + 1);

  if (isFirstword) {
    for (auto &pair : commands) {
      if (pair.first.starts_with(currentWord)) {
        matches.push_back(pair.first);
      }
    }

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

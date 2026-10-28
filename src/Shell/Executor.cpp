#include "shell.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {

bool applyRedirects(const std::vector<Redirect> &redirects, int &savedIn,
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

void restoreRedirects(int savedIn, int savedOut, int savedErr) {
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

std::string describePipeline(const Pipeline &pl) {
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

std::string describeAndOr(const AndOr &ao) {
  std::string out = describePipeline(ao.first);
  for (auto &[op, pl] : ao.rest)
    out += (op == LogicalOp::And ? " && " : " || ") + describePipeline(pl);
  return out;
}

} // namespace

pid_t Shell::forkAndExec(const Command &cmd, const std::vector<int> &allPipeFds,
                         int pipeIn, int pipeOut) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    // Restore default dispositions so Ctrl-C/Ctrl-Z affect children
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    // Put child in its own process group
    setpgid(0, 0);

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

  // Parent: ensure child's process group is set (race-safe; child also does
  // setpgid)
  setpgid(pid, pid);
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
    if (inBackground || !isatty(STDIN_FILENO)) {
      int status = 0;
      waitpid(pid, &status, WUNTRACED);
      if (WIFSTOPPED(status)) {
        process.addBackgroundJob(pid, pid, describePipeline(Pipeline{{cmd}}));
        Job *j = process.findBYPID(pid);
        if (j)
          j->status = JobStatus::Stopped;
      }
      return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    // Foreground: give terminal to child so Ctrl-C/Z go to it, not shell
    foregroundPgid = pid;
    tcsetpgrp(STDIN_FILENO, pid);
    int status = 0;
    waitpid(pid, &status, WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpgrp());
    foregroundPgid = -1;
    if (WIFSTOPPED(status)) {
      process.addBackgroundJob(pid, pid, describePipeline(Pipeline{{cmd}}));
      // Update status to Stopped (addBackgroundJob creates Running, fix it)
      Job *j = process.findBYPID(pid);
      if (j) {
        j->status = JobStatus::Stopped;
        std::cout << "[" << 1 << "] Stopped   "
                  << describePipeline(Pipeline{{cmd}}) << std::endl;
      }
      return 1;
    }
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
    // Join pipeline to first pid's process group
    if (i == 0) {
      setpgid(pid, pid);
    } else {
      setpgid(pid, pids[0]);
    }
    pids.push_back(pid);
  }

  for (int fd : pipeFds)
    close(fd);

  if (inBackground || !isatty(STDIN_FILENO)) {
    int status = 0;
    for (size_t i = 0; i < pids.size(); i++) {
      int st = 0;
      waitpid(pids[i], &st, WUNTRACED);
      if (i == pids.size() - 1 && WIFEXITED(st))
        status = WEXITSTATUS(st);
    }
    return status;
  }

  // Foreground pipeline: give terminal to its process group
  pid_t pgid = pids[0];
  foregroundPgid = pgid;
  tcsetpgrp(STDIN_FILENO, pgid);

  int status = 0;
  bool stopped = false;
  for (size_t i = 0; i < pids.size(); i++) {
    int st = 0;
    waitpid(pids[i], &st, WUNTRACED);
    if (WIFSTOPPED(st)) {
      stopped = true;
    }
    if (i == pids.size() - 1) {
      if (WIFEXITED(st))
        status = WEXITSTATUS(st);
      else if (WIFSIGNALED(st))
        status = 128 + WTERMSIG(st);
      else if (WIFSTOPPED(st))
        status = 1;
    }
  }

  tcsetpgrp(STDIN_FILENO, getpgrp());
  foregroundPgid = -1;

  if (stopped) {
    // Add pipeline as a stopped job
    std::string desc = describePipeline(pl);
    process.addBackgroundJob(pgid, pgid, desc);
    Job *j = process.findBYPID(pgid);
    if (!j)
      j = process.findByPGID(pgid);
    if (j)
      j->status = JobStatus::Stopped;
    std::cout << "[" << 1 << "] Stopped   " << desc << std::endl;
    return 1;
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
        inBackground = true;
        // Background job should not have terminal control
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        int s = executeAndOr(andOr);
        _exit(s);
      }
      process.addBackgroundJob(pid, describeAndOr(andOr));
    } else {
      executeAndOr(andOr);
    }
  }
}

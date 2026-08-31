#include "process.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

bool Process::exec(std::vector<std::string> &args) {
  auto amp = std::remove(args.begin(), args.end(), "&");
  bool background = (amp != args.end());
  args.erase(amp, args.end());

  if (args.empty()) {
    return true;
  }

  std::vector<char *> argv;
  for (auto &s : args) {
    argv.push_back(s.data());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return true;
  }

  if (pid == 0) {
    setpgid(0, 0);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);

    execvp(args[0].c_str(), argv.data());
    if (errno == ENOENT) {
      std::cerr << args[0] << ": command not found" << std::endl;
    } else {
      perror("execvp");
    }
    _exit(1);
  }

  setpgid(pid, pid);
  std::string cmd;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) {
      cmd += ' ';
    }
    cmd += args[i];
  }

  if (!background) {
    tcsetpgrp(STDIN_FILENO, pid);
    int status;
    waitpid(pid, &status, WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpgrp());
    if (WIFSTOPPED(status)) {
      jobs_.push_back(Job{pid, pid, cmd, JobStatus::Stopped, WSTOPSIG(status)});
      std::cout << "[" << jobs_.size() << "] Stopped " << cmd << std::endl;
    }
    return true;
  }
  jobs_.push_back(Job{pid, pid, cmd, JobStatus::Running, 0});
  std::cout << "[" << jobs_.size() << "] " << pid << std::endl;
  return true;
}

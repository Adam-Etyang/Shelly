#include "process.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <iostream>

bool Process::exec(std::vector<std::string>& args) {
  auto amp = std::remove(args.begin(), args.end(), "&");
  bool background = (amp != args.end());
  args.erase(amp, args.end());

  if (args.empty()) {
    return true;
  }

  std::vector<char*> argv;
  for (auto& s : args) {
    argv.push_back(s.data());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return true;
  }

  if (pid == 0) {
    execvp(args[0].c_str(), argv.data());
    if (errno == ENOENT) {
      std::cerr << args[0] << ": command not found" << std::endl;
    } else {
      perror("execvp");
    }
    _exit(1);
  }

  if (!background) {
    int status;
    waitpid(pid, &status, 0);
    return true;
  }

  std::string cmd;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) {
      cmd += ' ';
    }
    cmd += args[i];
  }
  jobs_.push_back(Job{pid, cmd, false});
  std::cout << "[" << jobs_.size() << "] " << pid << std::endl;
  return true;
}

void Process::reapJobs() {
  for (size_t i = 0; i < jobs_.size(); ++i) {
    Job& job = jobs_[i];
    if (job.done) {
      continue;
    }
    int status;
    pid_t r = waitpid(job.pid, &status, WNOHANG);
    if (r == job.pid) {
      job.done = true;
      std::cout << "[" << i + 1 << "] Done    " << job.command << std::endl;
    } else if (r == -1 && errno == ECHILD) {
      job.done = true;
    }
  }
}

void Process::printJobs() {
  reapJobs();
  if (jobs_.empty()) {
    std::cout << "no jobs" << std::endl;
    return;
  }
  for (size_t i = 0; i < jobs_.size(); ++i) {
    std::cout << "[" << i + 1 << "] " << (jobs_[i].done ? "Done    " : "Running ") << jobs_[i].pid << "  " << jobs_[i].command << std::endl;
  }
}
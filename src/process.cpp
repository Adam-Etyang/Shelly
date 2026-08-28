#include "process.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <iostream>
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
    tcsetgprp(STDIN_FILENO, getpgrp());
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

void Process::reapJobs() {
  for (size_t i = 0; i < jobs.size(); ++i) {
    Job &job = jobs_[i];
    if (jobs.status == JobStatus::Done) {
      continue;
    }
    int status;
    pid_t r = waitpid(job.pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if (r == job.pid) {
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        jobs.status = JobStatus::Done;
        jobs.lastStatus = status;
        std::cout << "[" << i + 1 << "] Done " << job.command << std::endl;
      } else if (WIFSTOPPED(status)) {
        jobs.status = JobStatus::Stopped;
        jobs.lastStatus = WSTOPSIG(status);
        std::cout << "[" << i + 1 << "] Stopped " << job.command << std::endl;
      } else if (WIFCONTINUED(status)) {
        jobs.status = JobStatus::Running;
        std::cout << "[" << i + 1 << "] RUnning " << job.command << std::endl;
      } else if (r == -1 && errno != ECHILD) {
        job.status = JobStatus::Done;
        std::cout << "[" << i + 1 << "] Done    " << job.command << std::endl;
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
      const char *label = jobs_[i].status == JobStatus::Done      ? "Done    "
                          : jobs_[i].status == JobStatus::Stopped ? "Stopped "
                                                                  : "Running ";
      std::cout << "[" << i + 1 << "] " << label << jobs_[i].pid << "  "
                << jobs_[i].command << std::endl;
    }
  }

  void Process::updateState(pid_t pid, JobStatus status, int lastStatus) {
    for (auto &job : jobs_) {
      if (job.pid == pid) {
        job.status = status;
        job.lastStatus = lastStatus;
        return;
      }
    }
  }

  Job *Process::findBySpec(const std::string &spec) {
    if (spec.empty()) {
      return nullptr;
    }
    if (spec[0] == '%') {
      std::string rest = spec.substr(1);
      if (rest == "+") {
        return currentJob();
      }
      if (rest == "-") {
        return previousJob();
      }
      try {
        size_t index = std::stoul(rest);
        if (index == 0 || index > jobs_.size()) {
          return nullptr;
        }
        return &jobs_[index - 1];
      } catch (const std::invalid_argument &) {
        return nullptr;
      } catch (const std::out_of_range &) {
        return nullptr;
      }
    }
    try {
      pid_t pid = std::stoi(spec);
      return findBYPID(pid);
    } catch (const std::invalid_argument &) {
      return nullptr;
    } catch (const std::out_of_range &) {
      return nullptr;
    }
  }

  Job *Process::currentJob() {
    if (jobs_.empty()) {
      return nullptr;
    }
    return &jobs_.back();
  }

  Job *Process::previousJob() {
    if (jobs_.size() < 2) {
      return nullptr;
    }
    return &jobs_[jobs_.size() - 2];
  }

  Job *Process::findBYPID(pid_t pid) {
    for (auto &job : jobs_) {
      if (job.pid == pid) {
        return &job;
      }
    }
    return nullptr;
  }

  Job *Process::findByPGID(pid_t pgid) {
    for (auto &job : jobs_) {
      if (job.pgid == pgid) {
        return &job;
      }
    }
    return nullptr;
  }


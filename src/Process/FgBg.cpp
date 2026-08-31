#include "process.hpp"
#include <csignal>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

bool Process::fg(const std::string &spec) {
  Job *job = spec.empty() ? currentJob() : findBySpec(spec);
  if (!job) {
    std::cerr << "fg: " << (spec.empty() ? "current" : spec) << ": no such job"
              << std::endl;
    return false;
  }
  if (job->status == JobStatus::Done) {
    std::cerr << "fg: job has terminated" << std::endl;
    return false;
  }

  pid_t pgid = job->pgid ? job->pgid : job->pid;
  if (job->status == JobStatus::Stopped) {
    kill(-pgid, SIGCONT);
    job->status = JobStatus::Running;
  }

  tcsetpgrp(STDIN_FILENO, pgid);
  int status;
  // wait for the job's process group leader; reap will handle exit/stop
  pid_t r = waitpid(job->pid, &status, WUNTRACED);
  tcsetpgrp(STDIN_FILENO, getpgrp());

  if (r == -1) {
    perror("waitpid");
    return false;
  }

  if (WIFSTOPPED(status)) {
    job->status = JobStatus::Stopped;
    job->lastStatus = WSTOPSIG(status);
    std::cout << "[" << (job - &jobs_[0] + 1) << "] Stopped   " << job->command
              << std::endl;
  } else {
    job->status = JobStatus::Done;
    job->lastStatus = status;
    std::cout << job->command << std::endl;
  }
  return true;
}

bool Process::bg(const std::string &spec) {
  Job *job = spec.empty() ? currentJob() : findBySpec(spec);
  if (!job) {
    std::cerr << "bg: " << (spec.empty() ? "current" : spec) << ": no such job"
              << std::endl;
    return false;
  }
  if (job->status == JobStatus::Done) {
    std::cerr << "bg: job has terminated" << std::endl;
    return false;
  }
  if (job->status == JobStatus::Running) {
    std::cerr << "bg: job already running" << std::endl;
    return false;
  }

  pid_t pgid = job->pgid ? job->pgid : job->pid;
  kill(-pgid, SIGCONT);
  job->status = JobStatus::Running;
  std::cout << "[" << (job - &jobs_[0] + 1) << "] " << job->command << " &"
            << std::endl;
  return true;
}

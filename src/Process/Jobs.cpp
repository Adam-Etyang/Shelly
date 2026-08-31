#include "process.hpp"
#include <cerrno>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

void Process::addBackgroundJob(pid_t pid, const std::string &command) {
  addBackgroundJob(pid, pid, command);
}

void Process::addBackgroundJob(pid_t pid, pid_t pgid,
                               const std::string &command) {
  pgid = pgid ? pgid : pid;
  jobs_.push_back(Job{pid, pgid, command, JobStatus::Running, 0});
  std::cout << "[" << jobs_.size() << "] " << pgid << std::endl;
}

void Process::reapJobs() {
  for (size_t i = 0; i < jobs_.size(); ++i) {
    Job &job = jobs_[i];
    if (job.status == JobStatus::Done) {
      continue;
    }
    int status;
    pid_t r = waitpid(job.pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if (r == job.pid) {
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job.status = JobStatus::Done;
        job.lastStatus = status;
        std::cout << "[" << i + 1 << "] Done    " << job.command << std::endl;
      } else if (WIFSTOPPED(status)) {
        job.status = JobStatus::Stopped;
        job.lastStatus = WSTOPSIG(status);
        std::cout << "[" << i + 1 << "] Stopped " << job.command << std::endl;
      } else if (WIFCONTINUED(status)) {
        job.status = JobStatus::Running;
        std::cout << "[" << i + 1 << "] Running " << job.command << std::endl;
      }
    } else if (r == -1 && errno == ECHILD) {
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

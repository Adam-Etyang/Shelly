#include "process.hpp"
#include <stdexcept>
#include <string>

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
    if (rest.empty() || rest == "%" || rest == "+") {
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
      // fall through to prefix search
    } catch (const std::out_of_range &) {
      return nullptr;
    }
    // %prefix search
    for (auto &job : jobs_) {
      if (job.command.rfind(rest, 0) == 0) {
        return &job;
      }
    }
    return nullptr;
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
  // most recent non-Done job
  for (int i = (int)jobs_.size() - 1; i >= 0; --i) {
    if (jobs_[i].status != JobStatus::Done) {
      return &jobs_[i];
    }
  }
  if (jobs_.empty()) {
    return nullptr;
  }
  return &jobs_.back();
}

Job *Process::previousJob() {
  int found = 0;
  for (int i = (int)jobs_.size() - 1; i >= 0; --i) {
    if (jobs_[i].status != JobStatus::Done) {
      if (found == 1) {
        return &jobs_[i];
      }
      found++;
    }
  }
  return nullptr;
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

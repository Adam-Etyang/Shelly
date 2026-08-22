#pragma once
#include <string>
#include <sys/types.h>
#include <vector>

struct Job {
  pid_t pid;
  std::string command;
  bool done = false;
};

class Process {
public:
  bool exec(std::vector<std::string>& args);
  void addBackgroundJob(pid_t pid, const std::string &command);
  void reapJobs();
  void printJobs();

private:
  std::vector<Job> jobs_;
};
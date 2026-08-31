#pragma once
#include <string>
#include <sys/types.h>
#include <vector>

enum class JobStatus { Running, Done, Stopped };

struct Job {
  pid_t pid;
  pid_t pgid;
  std::string command;
  JobStatus status;
  int lastStatus;
};

class Process {
public:
  bool exec(std::vector<std::string> &args);
  void addBackgroundJob(pid_t pid, const std::string &command);
  void addBackgroundJob(pid_t pid, pid_t pgid, const std::string &command);
  void reapJobs();
  void printJobs();

  Job *findBySpec(const std::string &spec);
  Job *currentJob();
  Job *previousJob();
  Job *findBYPID(pid_t pid);
  Job *findByPGID(pid_t pgid);
  void updateState(pid_t pid, JobStatus status, int lastStatus);

  bool fg(const std::string &spec);
  bool bg(const std::string &spec);

private:
  std::vector<Job> jobs_;
};

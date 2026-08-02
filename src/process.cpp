#include "process.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>

bool Process::exec(std::vector<std::string>& args){
      std::vector<char*> argv;
    for(auto& s: args){
        argv.push_back(s.data());
    }
    argv.push_back(nullptr);
    pid_t pid = fork();
    if(pid == -1 ){
        perror("fork");
        return true;
    }
    if(pid == 0){
        execvp(args[0].c_str(), argv.data());
        if(errno == ENOENT){
            std::cerr << args[0] << ": command not found" << std::endl;
        }
        else{
        perror("execvp");
        }
        _exit(1);
    }else{
        int status;
        waitpid(pid, &status, 0);
    }
    return true;

}


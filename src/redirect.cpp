#include "builtins.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>

int Builtins::redirect(std::vector<std::string>& args){
      auto redirect_it =std::find(args.begin(),args.end(), ">");
    if(redirect_it == args.end()){
        redirect_it = std::find(args.begin(), args.end(), "1>");
    }
    
    if(redirect_it == args.end()){
        return -1;
    }
    std::vector<std::string> cmd_args(args.begin(), redirect_it);
    std::vector<std::string> out_files (redirect_it+1, args.end());
    if(!out_files.empty()){
        int  file_fd = open(out_files[0].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(file_fd < 0){
            perror("open");
            return -1;
        }
        int save_out = dup(STDOUT_FILENO);
        dup2(file_fd, STDOUT_FILENO);
        close(file_fd);
        args = std::move(cmd_args);
        return save_out;
        
    }
    return -1;

}

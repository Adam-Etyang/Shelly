#include "builtins.hpp"
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <filesystem>

static void Builtins::cd(std::vector<std::string>& arr){
    std::string newpath; 
    if (args.size() < 2){
        const char* home = std::getenv("HOME");
        if(!home){
            std::cout<< "home dir not set" << std::endl;
            return;
        }
        newpath = home;
    }else{
        newpath = args[1];
    }
    if(!newpath.empty() && newpath[0] == '~'){
        const char* home = std::getenv("HOME");
        if(!home){
            std::cout << "Home dir not set"<< std::endl;
            return;
        }
        newpath = home + newpath.substr(1);
    }
    try{
        std::filesystem::current_path(newpath);
    }
    catch(const std::filesystem::filesystem_error& e){
        std::cout<< "cd: " << newpath << ": No such file or directory" << std::endl;
    }

}





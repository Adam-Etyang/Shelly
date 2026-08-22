#include "builtins.hpp"
#include <iostream>

int Builtins::echo(std::vector<std::string>& args){
    for(size_t i = 1; i < args.size(); i++){
        std::cout << args[i];
        if(i+1 < args.size()){
            std::cout <<" ";
        }
    }
    std::cout << std::endl;
    return 0;
}

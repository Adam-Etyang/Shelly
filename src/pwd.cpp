#include "builtins.hpp"
#include <iostream>
#include <filesystem>



void Builtins::pwd(std::vector<std::string>& args){
    std::filesystem::path current_dir = std::filesystem::current_path();
    std::cout<< current_dir.string()<< std::endl; 
}

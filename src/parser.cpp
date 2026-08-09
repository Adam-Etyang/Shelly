#include "parser.hpp"
#include <cctype>


std::vector<std::string> Parser::tokenize(std::string_view line){
    bool singlequote = false;
    bool doublequote = false;
    std::string current;
    std::vector<std::string>args;

    for(size_t i = 0; i<line.size();i++){
        char c = line[i];
        if(singlequote){
            if(c == '\''){
                singlequote = false;
            }else{
                current +=c;
            }
        }else if(doublequote){
            if(c == '"'){
                doublequote = false;
            }else if(c == '\\' && i+1<line.size()&& (line[i+1] == '"' ||line[i+1] == '\\' || line[i+1] == '$')){
                current += line[i+1];
                i++;
            }else{
                current += c;
            }
        }else{
            if(c == '\''){
                singlequote = true;
            }else if(c == '"'){
                doublequote = true;
            }else if(c == '\\' && i+1 < line.size()){
                current += line[i+1];
                i++;
            }else if(std::isspace(static_cast<unsigned char>(c))){
                if(!current.empty()){
                    args.push_back(current);
                    current.clear();
                }
            }else{
                current += c;
            }
        }
    }
    if(!current.empty()){
        args.push_back(current);
    }
    return args;
}


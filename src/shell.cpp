#include "Shell.hpp"
#include "Parser.hpp"

Shell :: Shell() {
  commands["cd"] = [this](std::vector<std::string>& args){return cd(args);};
  commands["echo"] = [this](std::vector<std::string>& args) {return echo(args);};
  commands["pwd"] = [this](std::vector<std::string>& args){return pwd(args);};
  commands["type"] = [this](std::vector<std::string>& args){return type(args);};

}

void Shell::run() {
  Parser parser;
  while(true){
    std::cout << "$ ";
    std::string line;
    if(!std::getline(std::cin, line)) break;
    auto args = parser.tokenize(line);
    if(args.empty()) continue;
    if(args[0] == "exit")break;
    
    int saved_out = redirect(args); 
    auto it = commands.find(args[0]);
    if(it != commands.end()){
        it -> second(args);
    }else{
        exec(args);
    }
    if(saved_out != -1){
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    
    
}
}

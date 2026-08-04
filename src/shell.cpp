#include "Shell.hpp"
#include "builtins.hpp"

Shell :: Shell() {
  commands["cd"] = builtins::cd;
  commands["echo"] = builtins::echo;
  commands["pwd"] = builtins::pwd;
  commands["type"] = builtins::type;
}

void Shell::run() {

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
        process.exec(args);
    }
    if(saved_out != -1){
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    
    
}
}

void Shell::register_builtin(const std::string& name, CommandFunc func) {
  commands[name] = func;
}


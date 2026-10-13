#include<iostream>
#include<unistd.h>
#include<string>
#include<vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include<sys/wait.h>
#include<signal.h>
#include<cerrno>
using namespace std;
// >>> resets the redirection instead of error- so need to handle this
// implement redirection in ls and pwd also 
// make the mkdir command work on multiple files
// make parser also parse ""    - done 
// update parser for pipes  - done 

// int startingID = 1;
// struct backgroundChildren{
//     int id;
//     int pid;
// };

volatile sig_atomic_t childChanged = 0;
pid_t shell_pgrp_id;
void reapChild(){
    if(childChanged){
        childChanged = 0;
        int status;
        int pid;
        while((pid = waitpid(-1,&status,WNOHANG))>0){
            cout<<"Child "<<pid<<' '<<"completed"<<flush;    // need to make changes here since real shell doesnt print completed message
            cout<<endl<<flush;
        }
    }
}
struct Command{
    string name;
    vector<string>args;
    vector<string>outputFiles;
    vector<string>modes;
};
struct Pipeline{
    vector<Command>commands;
    bool background = false;
};
void pwd(){
    char *c = getcwd(nullptr,0);
    cout<<c<<endl;
    free(c);
}

void ls(){
    DIR* currdir = opendir(".");
    if(!currdir){
        perror("opendir");
        cout<<endl;
        return;
    }
    dirent* d = readdir(currdir);
    while(d!=nullptr){
        string fileName = d->d_name;
        if(fileName[0]!='.') cout<<d->d_name<<endl;
        d = readdir(currdir);
    }
    closedir(currdir);
}

void cd(Command &token){
    if(token.args.empty()){
        cerr<<"cd: missing operand"<<endl;
        return;
    }
    string path = token.args[0];
    int cd = chdir(path.c_str());
    if(cd==-1) perror("cd");
}
void makedir(Command &token){
    if(token.args.empty()){
        cerr<<"mkdir: missing operand"<<endl;
        return;
    }
    string path = token.args[0];
    int status = mkdir(path.c_str(),0777);
    if(status==-1) perror("mkdir");
}

void removedir(Command &token){
    if(token.args.empty()){
        cerr<<"rmdir: missing operand"<<endl;
        return;
    }
    string path = token.args[0];
    int status = rmdir(path.c_str());
    if(status==-1) perror("rmdir");
}

void callOuterCommands( Pipeline &pipeline){
    pid_t group_pgid = -1;
    int lastPipeRead = -1;
    vector<pid_t>chilren_pids;
    bool background = pipeline.background;
    for(int i=0;i<pipeline.commands.size();i++){
        int fd[2];
        if(i!=pipeline.commands.size()-1){
            if(pipe(fd)==-1){
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
        pid_t child = fork();
        if(child==0){
            // child proces
            signal(SIGTTOU, SIG_DFL);
            pid_t pgid = group_pgid==-1 ? getpid() : group_pgid;
            if(setpgid(0,pgid)==-1) perror("setpgid: child");   // makes the child join a new process group
            if(i!=pipeline.commands.size()-1){
                close(fd[0]);
                if(lastPipeRead != -1){
                    if(dup2(lastPipeRead,STDIN_FILENO)==-1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(lastPipeRead);
                } 
                if(dup2(fd[1],STDOUT_FILENO)==-1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd[1]);
            }
            else if(i!=0 && i==pipeline.commands.size()-1){
                if(lastPipeRead != -1){
                    if(dup2(lastPipeRead,STDIN_FILENO)==-1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(lastPipeRead);
                }
            }
            Command token = pipeline.commands[i];
            vector<char*> arr;
            arr.push_back(token.name.data());
            for(string &i:token.args){
                arr.push_back(i.data());
            }
            arr.push_back(nullptr);

            int fd=-1;
            for(int i=0;i<token.outputFiles.size();i++){
                if(fd!=-1) close(fd);
                if(token.modes[i]=="append") fd = open(token.outputFiles[i].data(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                else fd = open(token.outputFiles[i].data(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd==-1){
                    perror("open");
                    exit(EXIT_FAILURE);
                }
            }
            if(fd!=-1){
                if(dup2(fd, STDOUT_FILENO)==-1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                };
                close(fd);
            }
            execvp(token.name.data(), arr.data());
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if(child>0){
            pid_t pgid = group_pgid==-1 ? child : group_pgid;
            if(setpgid(child,pgid)==-1) perror("setpgid: parent");
            group_pgid = pgid;
            chilren_pids.push_back(child);
            if(i!=pipeline.commands.size()-1){
                close(fd[1]);   
                if(lastPipeRead!=-1) close(lastPipeRead);
                lastPipeRead = fd[0];
            }
            else{
                if(lastPipeRead!=-1)close(lastPipeRead);
                if(!background){
                    tcsetpgrp(STDIN_FILENO,group_pgid);
                    for(auto &c:chilren_pids) waitpid(c,nullptr,WUNTRACED);
                    tcsetpgrp(STDIN_FILENO,shell_pgrp_id);
                }
            }
        }
        else perror("fork");
    }
}
void commandSelector(Command &token, Pipeline &pipeline){
    string command = token.name;
    if(command == "pwd") pwd();
    else if(command == "ls") ls();
    else if(command == "cd") cd(token);
    else if(command == "mkdir") makedir(token);
    else if(command == "rmdir") removedir(token);
    else callOuterCommands(pipeline);
}

void inputParser(string input){
    Command token;
    string ele;
    int mode =0; // 0 means write to ele , 1 means write to ouput file, 2 means append to ouput file
    Pipeline pipeline;
    for(int i=0;i<input.size();i++){
        char s = input[i];
        if(s==' ' || s=='>' || s=='\'' || s=='"' || s=='|' || s=='&'){
            if(!ele.empty()){
                if(token.name.empty()) token.name=ele;
                else{
                    if(mode==0) token.args.push_back(ele);
                    else{
                        if(ele.empty()) continue;
                        token.outputFiles.push_back(ele);
                        string m = mode==1 ? "write" : "append";
                        token.modes.push_back(m); 
                        mode = 0;
                    }
                }
                ele.clear();
            }
            if(s=='|'){
                if(token.name.empty()){
                    cout<<"Error: No command before Pipe"<<endl;
                    return;
                }
                pipeline.commands.push_back(token);
                token = Command();
            }
            else if(s=='\''){
                i++;
                bool closed = false;
                while(i<input.size()){
                    if(input[i]=='\''){
                        closed=true;
                        break;
                    }
                    ele+=input[i];
                    i++;
                }
                if(!closed){
                    cout<<"Error: unmatched single quote\n";
                    return;
                }
            }
            else if(s=='"'){
                i++;
                bool closed = false;
                while(i<input.size()){
                    if(input[i]=='"'){
                        closed=true;
                        break;
                    }
                    ele+=input[i];
                    i++;
                }
                if(!closed){
                    cout<<"Error: unmatched double quote\n";
                    return;
                }
            }
            else if(s=='>'){
                if(mode==2){
                    cout<<"Redirection: more than 2 > recieved"<<endl;
                    return;
                }
                else mode++;
            }
            else if(s=='&') pipeline.background = true;
        }
        else ele+=s;
    }
    if(!ele.empty()){
        if(token.name.empty()) token.name=ele;
        else{
            if(mode==0) token.args.push_back(ele);
            else{
                token.outputFiles.push_back(ele);
                string m = mode==1 ? "write" : "append";
                token.modes.push_back(m); 
            }
        }
        ele.clear();
    }
    // call operations
    if(!token.name.empty()) pipeline.commands.push_back(token);
    else if(!pipeline.commands.empty()){
        cout<<"Error: No Command after pipe"<<endl;
        return;
    }
    else return;
    if(pipeline.commands.empty()) return;
    commandSelector(token, pipeline);
    // cout<<token.size()<<endl;
}


void sigchldHandler(int signal){
    childChanged = 1;
}

bool readInput(string &input) {
    input.clear();
    char ch;
    while (true) {
        ssize_t bytesRead = read(STDIN_FILENO, &ch, 1);
        if(bytesRead == 1) {
            if(ch=='\n') {
                return true;
            }
            input+=ch;
        }
        else if(bytesRead == 0) {
            return !input.empty();
        }
        else{
            if(errno == EINTR){
                reapChild();                // when no foreground process running and we are in read state
                cout<<"my_shell>: "<<flush;
                continue;
            }
            perror("read");
            return false;
        }
    }
    return false;
}
int main(int argc, char const *argv[])
{
    signal(SIGTTOU, SIG_IGN);
    struct sigaction sa{};
    sa.sa_handler = sigchldHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD,&sa, NULL);
    shell_pgrp_id = getpgrp();
    while(true){
        reapChild();                  // this is for the case when there is some foreground process running
        cout<<"my_shell>: "<<flush;
        string input;
        if (!readInput(input)) {
            break;
        }
        if(input =="q" || input=="exit") break;
        else{
            inputParser(input);
        }
    }
    return 0;
}

// add both these below to readme
// current prob => waitpid of background runs for both background and foreground processes
// sol => maintain a list of background processes to run it for only background

// current prob => my_shell appears in same line as ^C
// sol => make a flag set it when child is stopped and print new line when flag is set

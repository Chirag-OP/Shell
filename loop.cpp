#include<iostream>
#include<unistd.h>
#include<string>
#include<vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include<sys/wait.h>
using namespace std;
// >>> resets the redirection instead of error- so need to handle this
// implement redirection in ls and pwd also 
// make the mkdir command work on multiple files
// make parser also parse ""    - done 
// update parser for pipes  - done 
struct Command{
    string name;
    vector<string>args;
    vector<string>outputFiles;
    vector<string>modes;
};
struct Pipeline{
    vector<Command>commands;
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
    int lastPipeRead = -1;
    vector<pid_t>chilren_pids;
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
            chilren_pids.push_back(child);
            if(i!=pipeline.commands.size()-1){
                close(fd[1]);   
                if(lastPipeRead!=-1) close(lastPipeRead);
                lastPipeRead = fd[0];
            }
            else{
                if(lastPipeRead!=-1)close(lastPipeRead);
                for(auto &c:chilren_pids) waitpid(c,nullptr,0);
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
        if(s==' ' || s=='>' || s=='\'' || s=='"' || s=='|'){
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
    else{
        cout<<"Error: No Command after pipe"<<endl;
        return;
    }
    if(pipeline.commands.empty()) return;
    commandSelector(token, pipeline);
    // cout<<token.size()<<endl;
}
int main(int argc, char const *argv[])
{
    while(true){
        cout<<"my_shell>: ";
        string input;
        getline(cin,input);
        if(input =="q" || input=="exit") break;
        else{
            inputParser(input);
        }
    }
    return 0;
}

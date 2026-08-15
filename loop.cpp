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

// implement redirection in ls and pwd also 
// make the mkdir command work on multiple files
// make parser also parse "" 
struct Command{
    string name;
    vector<string>args;
    vector<string>outputFiles;
    vector<string>modes;
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

void callOuterCommands(Command &token){
    pid_t child = fork();
    vector<char*> arr;
    arr.push_back(token.name.data());
    for(string &i:token.args){
        arr.push_back(i.data());
    }
    arr.push_back(nullptr);
    if(child==0){
        // child proces
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
    else if(child>0) waitpid(child,nullptr,0);
    else perror("fork");
}
void commandSelector(Command &token){
    string command = token.name;
    if(command == "pwd") pwd();
    else if(command == "ls") ls();
    else if(command == "cd") cd(token);
    else if(command == "mkdir") makedir(token);
    else if(command == "rmdir") removedir(token);
    else callOuterCommands(token);
}

void inputParser(string input){
    Command token;
    string ele;
    int mode =0; // 0 means write to ele , 1 means write to ouput file, 2 means append to ouput file
    for(char &s:input){
        if(s==' ' || s=='>'){
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
            if(s=='>'){
                if(mode==2) mode=0;
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
    if(token.name.empty()) return;
    commandSelector(token);
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

#include<iostream>
#include<unistd.h>
#include<string>
#include<vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
using namespace std;

struct Command{
    string name;
    vector<string>args;
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

void commandSelector(Command &token){
    string command = token.name;
    if(command == "pwd") pwd();
    else if(command == "ls") ls();
    else if(command == "cd") cd(token);
    else if(command == "mkdir") makedir(token);
    else if(command == "rmdir") removedir(token);
    else cerr<<token.name<<": command not found"<<endl;
}

void inputParser(string input){
    Command token;
    string ele;
    for(char &s:input){
        if(s==' '){
            if(!ele.empty()){
                if(token.name.empty()) token.name=ele;
                else token.args.push_back(ele);
                ele.clear();
            }
        }
        else ele+=s;
    }
    if(!ele.empty()){
        if(token.name.empty()) token.name=ele;
        else token.args.push_back(ele);
        ele.clear();
    }
    // call operations
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

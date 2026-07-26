#include<iostream>
#include<unistd.h>
#include<string>
#include<vector>
#include <sys/types.h>
#include <dirent.h>
using namespace std;
vector<string>token;
void simpleOperations(){
    string command = token[0];
    if(command =="pwd"){
        char *c = getcwd(nullptr,0);
        cout<<c<<endl;
        free(c);
    }
    else if(command =="ls"){
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
}
void twoOperations(){
    string command = token[0];
    string path = token[1];
    if(command == "cd"){
        int cd = chdir(path.c_str());
        if(cd==-1) perror("cd");
    }
}
void inputParser(string input){
    string ele;
    for(char &s:input){
        if(s==' '){
            if(!ele.empty()){
                token.push_back(ele);
                ele.clear();
            }
        }
        else ele+=s;
    }
    if(!ele.empty()){
        token.push_back(ele);
        ele.clear();
    }
    // call operations
    if(token.size()==1) simpleOperations();  
    else if(token.size()==2) twoOperations();
    // cout<<token.size()<<endl;
    token.clear();
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

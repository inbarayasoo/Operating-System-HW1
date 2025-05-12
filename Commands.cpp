#include "Commands.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace std;
const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY() \
    cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
    cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

//-----------Help Function ------------/

string _ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char*)malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool isNumeric(std::string str) {
    try {
        int i = std::stoi(str);
        // Optionally use 'i' if needed
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }
}

bool searchSpecialSign(const char *cmd_line, const string &c) {
    string cmd = cmd_line;
    if (cmd.find(c) != string::npos)
        return true;
    return false;
}

bool isComplexCommand(const char *cmd_line) {
    return searchSpecialSign(cmd_line, "*") || searchSpecialSign(cmd_line, "?");
}

bool isPipeCommand(const char *cmd_line){
  return searchSpecialSign(cmd_line, "|");
}

bool isRedirectionCommand(const char *cmd_line){
  return searchSpecialSign(cmd_line, ">") || searchSpecialSign(cmd_line, ">>");;
}


//------------SMASH CLASS-------------/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    SmallShell& shell = SmallShell::getInstance();
    char* argsVec[COMMAND_MAX_ARGS];
    Command* requestedCommand = nullptr;
    int argc = _parseCommandLine(cmd_line, argsVec);
    if (argc == 0) //if empty cmd
      return nullptr; 
    string commandName(argsVec[0]);

    //checking which special command
    if(isPipeCommand(cmd_line)){
        char* dupCommand = new char(strlen(cmd_line) + 1);
        strcpy(dupCommand, cmd_line);
        if (_isBackgroundComamnd(dupCommand)){
            _removeBackgroundSign(dupCommand);
        }
        requestedCommand = new PipeCommand(cmd_line);
    } else if(isRedirectionCommand(cmd_line)){
        char* dupCommand = new char(strlen(cmd_line) + 1);
        strcpy(dupCommand, cmd_line);
        if (_isBackgroundComamnd(dupCommand)){
            _removeBackgroundSign(dupCommand);
        } 
        requestedCommand = new RedirectionCommand(cmd_line);
    } else if(commandName == "chmod" || commandName == "chmod&") {
        requestedCommand = new ChmodCommand(cmd_line);
    }

    // checking which built-in command
    else if (commandName == "chprompt" || commandName == "chprompt&") {
        if (argc == 1)
            promptName = "smash";
        else
            promptName = argsVec[1];
        return nullptr;
    } else if (commandName == "showpid" || commandName == "showpid&") {
        requestedCommand = new ShowPidCommand(cmd_line);
    } else if (commandName == "pwd" || commandName == "pwd&") {
        requestedCommand = new GetCurrDirCommand(cmd_line);
    } else if (commandName == "fg" || commandName == "fg&") {
        requestedCommand = new ForegroundCommand(cmd_line, &shell.jobs);
    } else if (commandName == "cd" || commandName == "cd&") {
        if (getcwd(NULL, 0) == NULL) {
            perror("smash error: getcwd failed");
            return nullptr;
        }
        requestedCommand = new ChangeDirCommand(cmd_line, getcwd(NULL, 0));;
    } else if (commandName == "jobs" || commandName == "jobs&") {
        requestedCommand = new JobsCommand(cmd_line, &shell.jobs);
    } else if (commandName == "quit" || commandName == "quit&") {
        requestedCommand = new QuitCommand(cmd_line, &shell.jobs);
    } else if (commandName == "kill" || commandName == "kill&") {
        requestedCommand = new KillCommand(cmd_line, &shell.jobs);
    }
    else{ //it should be external
      requestedCommand = new ExternalCommand(cmd_line);
    }
    return requestedCommand;
}

void SmallShell::executeCommand(const char* cmd_line) {
    jobs.removeFinishedJobs();
    Command* command = CreateCommand(cmd_line);
    if (command == nullptr)
        return;
    command->execute();
}

//------------JobsList------------/
void JobsList::removeFinishedJobs(){
    int state;
    for (auto it = jobs.begin(); it != jobs.end();) {
        state = waitpid(it->jobPid, NULL, WNOHANG);
        if (state != -1 && state != it->jobPid)
            ++it;
        else if (state == it->jobPid)
            it = jobs.erase(it);
        else {
            if (errno != ECHILD)  
                perror("smash error: waitpid failed");
            return;
        }
    }
}

void JobsList::addJob(Command* cmd,int jobPid) {
    removeFinishedJobs();
    int id;
    if (!(jobs.empty()))
        id = jobs.back().jobId + 1;
    else
        id = 1;
    jobs.push_back(JobEntry(id,jobPid,cmd));
}

void JobsList::printJobsList(){
    removeFinishedJobs();
    sort(jobs.begin(), jobs.end(),
         [](const JobEntry& job1, const JobEntry& job2) { return job1.jobId < job2.jobId; });
    for (auto &job: jobs) {
        std::cout << "[" << job.jobId << "] " << job.cmd->cmd_line << std::endl;
    }
}

void JobsList::killAllJobs(){
    for (auto &job: jobs) {
        cout << job.jobPid << ": " << job.cmd->cmd_line << endl;
        if (kill(job.jobPid, SIGKILL) < 0)
            cerr <<"smash error: kill failed";
    }
}

void JobsList::removeJobById(int jobId) {
    for (auto it = jobs.begin(); it != jobs.end();) {
        if (it->jobId == jobId) {
            it = jobs.erase(it);
            return;
        } else
            ++it;
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId){
    for (auto &job: jobs){
      if (job.jobId == jobId){
          return &job;
      }
    }
    return nullptr;
}

//------------ShowPID------------/
void ShowPidCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    cout << "smash pid is " << shell.shellPid << endl;
}

//------------CD------------/
char* ChangeDirCommand::lastDir = nullptr;
void ChangeDirCommand::execute() { 
    char* argv[COMMAND_MAX_ARGS];
    int argc = _parseCommandLine(cmd_line.c_str(), argv);
    std::string nextDir = argv[1];

    if (argc > 2) {  
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    } else if (argc == 1)  
        return;
    else {
        if (nextDir != "-") {         
            if (chdir(nextDir.c_str()) < 0 ){
                perror("smash error: chdir failed");
                return;
            }
            lastDir = currentDir;
        } else {
            if (lastDir != nullptr) {
                    if (chdir(lastDir) < 0 ){
                        perror("smash error: chdir failed");
                        return;
                    }
                    lastDir = currentDir;
                } else
                    cerr << "smash error: cd: OLDPWD not set" << endl;
                return; 
        }
    }
}

//------------PWD------------/
void GetCurrDirCommand::execute(){
    char *currentDir = getcwd(NULL, 0);
    if (currentDir != nullptr) {
        cout << currentDir << endl;
        return;
    }
        perror("smash error: getcwd failed");
}

//--------------Jobs--------------/
void JobsCommand::execute() {
    jobs->printJobsList();
}

//--------------Fg--------------/
void ForegroundCommand::execute(){
    JobsList::JobEntry* job;
    char* argv[COMMAND_MAX_ARGS];
    SmallShell& shell = SmallShell::getInstance();
    int argc = _parseCommandLine(cmd_line.c_str(), argv);
    char* p;
    
    if(argc == 1){ //no id
        if(!(jobs->jobs.empty())){
            job = &jobs->jobs[0];
            for (auto& iteratedJob : jobs->jobs)        
                if (iteratedJob.jobId > job->jobId){
                    job = &iteratedJob;
                }
        } else {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
    } else if(argc >= 2){ //specific id
        int jobId = (int)strtol(argv[1], &p, 10);    // TO DO: can we move this line to the beginning of the function where there is all the parameters?
        if (*p) {                              // TO DO: check if you can do de-morgan on this if expression
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        job = jobs->getJobById(jobId);
        if(job == nullptr) {
            cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
            return;
        }
    }

    if (argc > 2) {  
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    
    cout << job->cmd->cmd_line << " " << job->jobPid << endl;
    job = new JobsList::JobEntry(*job);  
    shell.fgJob = job;
    jobs->removeJobById(job->jobId);
    if (waitpid(job->jobPid, NULL, WSTOPPED) == -1)
        perror("smash error: waitpid failed");
    shell.fgJob = nullptr;
}

//------------Kill------------/
void KillCommand::execute() {
    char* argv[COMMAND_MAX_ARGS];
    JobsList::JobEntry* job;
    int argc = _parseCommandLine(cmd_line.c_str(), argv);
    char* temp;

    if(argc >= 3) { 
        int id = (int)strtol(argv[2], &temp, 10);
        if(id == 0){
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
        job = jobs->getJobById(id);
        if (job == nullptr) {
            cerr << "smash error: kill: job-id " << id << " does not exist" << endl;
            return;
        }
        if (*temp) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
        }
    } 

    if (argc != 3 || argv[1][0] != '-') {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    int signum = (int)strtol(argv[1] + 1, &temp, 10);
    if (*temp) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    if (kill(job->jobPid, signum) == -1) {
        perror("smash error: kill failed");
        return;
    }        
    else        
        cout << "signal number " << signum << " was sent to pid " << job->jobPid << endl;
}

//------------Quit------------/
void QuitCommand::execute() {
    char* argv[COMMAND_MAX_ARGS];
    bool isKill = true;
    int argc = _parseCommandLine(cmd_line.c_str(), argv);
    if (strcmp(argv[1], "kill") != 0)
        isKill = false;

    if (argc >= 2 && isKill ) { //kill arg
        cout << "smash: sending SIGKILL signal to " << jobs->jobs.size() << " jobs:" << endl;
        jobs->killAllJobs();
    }
    exit(0); 
  }

//------------External------------/
void ExternalCommand::execute() {
    char* argv2[COMMAND_MAX_ARGS];
    char* command = nullptr;
    command = new char(cmd_line.size() + 1);
    strcpy(command, cmd_line.c_str());
    if (_isBackgroundComamnd(cmd_line.c_str())) {  //background command
        _removeBackgroundSign(command);
        int pid = fork();
        if (pid < 0) {
            perror("smash error: fork failed");
            return;
        } else if (pid > 0)         //parent
            SmallShell::getInstance().jobs.addJob(this,pid);
        else {                       //child
            if (setpgrp() < 0) {
                perror("smash error: setpgrp failed");
                return;
            } else {
                if (isComplexCommand(cmd_line.c_str())) {
                    const char *bashExec = "/bin/bash";
                    char *argv1[] = {(char *) "bash", (char *) "-c", command, nullptr};
                    if (execvp(bashExec, argv1) < 0)
                        perror("smash error: execvp failed");
                    exit(0);
                }
                _parseCommandLine(command, argv2);
                if (execvp(argv2[0], argv2) < 0)
                    perror("smash error: execvp failed");
                exit(0);
            }
        }
    } else {  //foreground command
        int pid = fork();
        if (pid < 0) {
            perror("smash error: fork failed");
            return;
        } else if (pid > 0) {        //parent
            SmallShell::getInstance().fgJob = new JobsList::JobEntry(-1, pid,this);
            if (waitpid(pid, NULL, WSTOPPED) < 0)
                perror("smash error: waitpid failed");
            SmallShell::getInstance().fgJob = nullptr;
        } else {                       //child
            if (setpgrp() < 0) {
                perror("smash error: setpgrp failed");
                return;
            } else {
                if (isComplexCommand(cmd_line.c_str())) {
                    const char *bashExec = "/bin/bash";
                    char *argv1[] = {(char *) "bash", (char *) "-c", command, nullptr};
                    if (execvp(bashExec, argv1) < 0)
                        perror("smash error: execvp failed");
                    exit(0);
                }
                _parseCommandLine(command, argv2);
                if (execvp(argv2[0], argv2) < 0)
                    perror("smash error: execvp failed");
                exit(0);
            }
        }

    }
}

//------------Redirection IO------------/ 
RedirectionCommand::RedirectionCommand(const char* dupCommand) : Command(dupCommand) {   
    size_t index = cmd_line.find('>');

    if((cmd_line.length() > index+1 && cmd_line[index+1] != '>')){
        appendFlag = false;
        cmd = SmallShell::getInstance().CreateCommand(cmd_line.substr(0, index).c_str());
        outPath = _trim(cmd_line.substr(index + 1));
    } else {
        appendFlag = true;
        cmd = SmallShell::getInstance().CreateCommand(cmd_line.substr(0, index).c_str());
        outPath = _trim(cmd_line.substr(index + 2));
    }
}

void RedirectionCommand::execute(){
    int pid = fork(); // make a child process that write the output of command
    int fd = -1;

    if(pid == 0){ //child
        if (setpgrp() < 0) {
            perror("smash error: setpgrp failed");
            return;
        }
        if(close(1) < 0){
            perror("smash error: close failed");
        }   
        if(appendFlag) 
            fd = open(outPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        else 
            fd = open(outPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644); 

        if (fd != -1) {
            cmd->execute();
            exit(0); //SHOULD BE EXIT ?
        } else {
            perror("smash error: open failed");
        }
    }
    else if (pid > 0) { //parent
        if(waitpid(pid, NULL, WUNTRACED) == -1)
            perror("smash error: waitpid failed");
    } else //failed fork
        perror("smash error: fork failed");
}

//------------ChMode------------/
void ChmodCommand::execute(){
    char* argv[COMMAND_MAX_ARGS];
    char* temp;
    int argc = _parseCommandLine(cmd_line.c_str(), argv);
    int newMode = (int)strtol(argv[1], &temp,8);
    if (*temp){
        cerr << "smash error: chmod: invalid arguments" << endl;
        return;
    } else if(argc != 3){
        cerr << "smash error: chmod: invalid arguments" << endl;
        return;
    }
    if(chmod(argv[2], newMode) < 0){
        cerr << "smash error: chmod  failed" << endl;
    }
}


//------------Pipe-Bonus------------/
PipeCommand::PipeCommand(const char* cmdLine) : Command(cmdLine) {
    SmallShell& smash = SmallShell::getInstance();
    size_t positionPipe = cmd_line.find('|');
    bool isANDsign = false ; 
    if (cmd_line[positionPipe + 1] != '&')
        isANDsign = true;
    if (isANDsign) {
        firstCommand = smash.CreateCommand(cmd_line.substr(0, positionPipe).c_str());
        secondCommand = smash.CreateCommand(cmd_line.substr(positionPipe + 1).c_str());
        stdType = 1 ;
    } else {
        firstCommand = smash.CreateCommand(cmd_line.substr(0, positionPipe).c_str());
        secondCommand = smash.CreateCommand(cmd_line.substr(positionPipe + 2).c_str());
        stdType = 2 ;
    }
}
void PipeCommand::execute() {
    if (firstCommand == nullptr || secondCommand == nullptr)
        return;
    int fd[2];
    if (pipe(fd) == -1) {
        perror("smash error: pipe failed");
        return;
    }
    int fork1 = fork();
    if (fork1 == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (fork1 == 0) {         // first child, THE WRITER
        if (setpgrp() < 0) {
            perror("smash error: setpgrp failed");
            return;
        }

        if (stdType == 1){
            if(dup2(fd[1], 1) == -1 ) {
                perror("smash error: dup2 failed");
                return;
            }
        } else {
            if (dup2(fd[1], 2) == -1) {
                perror("smash error: dup2 failed");
                return;
            }
        }

        if (close(fd[0]) == -1) {
            perror("smash error: close failed");
            return;
        }
        if (close(fd[1]) == -1) {
            perror("smash error: close failed");
            return;
        }
        firstCommand->execute();
        exit(0);
    }

    int fork2 = fork();
    if (fork2 == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (fork2 == 0) {  // second child THE READER
        if (setpgrp() < 0) {
            perror("smash error: setpgrp failed");
            return;
        }
        if (dup2(fd[0], 0) == -1) {
            perror("smash error: dup2 failed");
            return;
        }
        if (close(fd[0]) == -1) {
            perror("smash error: close failed");
            return;
        }
        if (close(fd[1]) == -1) {
            perror("smash error: close failed");
            return;
        }
        secondCommand->execute();
        exit(0);
    }

    if (close(fd[0]) == -1) {
        perror("smash error: close failed");
        return;
    }
    if (close(fd[1]) == -1) {
        perror("smash error: close failed");
        return;
    }

    while (true) {
        if (waitpid(-1, NULL, WNOHANG) == -1)
            break;
    }
}
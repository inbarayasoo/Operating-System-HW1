#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
class JobsList;

class Command {
  public:
    std::string cmd_line;

    Command(const char* cmd_line) : cmd_line(cmd_line) {}
    virtual ~Command() = default;
    virtual void execute() = 0;
};

class BuiltInCommand : public Command {
  public:
    BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
  public:
    ExternalCommand(const char* cmd_line) : Command(cmd_line) {}
    virtual ~ExternalCommand() {}
    void execute() override;
};

// BONUS
class PipeCommand : public Command {
  public:
    int stdType;
    Command* firstCommand;
    Command* secondCommand;

    PipeCommand(const char* cmd_line_ch);
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public Command {
public:
  explicit RedirectionCommand(const char* dupCommand);
  virtual ~RedirectionCommand() {}
  void execute() override;

  Command* cmd;
  bool appendFlag;
  std::string outPath;
};

class ChangeDirCommand : public BuiltInCommand {
  public:
    ChangeDirCommand(const char* cmd_line, char* curwd) : BuiltInCommand(cmd_line), currentDir(curwd) {}
    virtual ~ChangeDirCommand() {}
    void execute() override;

    static char* lastDir;
    char* currentDir;
};

class GetCurrDirCommand : public BuiltInCommand {
  public:
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
   public:
    ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
  public:
    JobsList* jobs;
    QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~QuitCommand() {}
    void execute() override;
};

class JobsList {
  public:
    class JobEntry {
        public:
        JobEntry(int jobId, int jobPid, Command *cmd) : jobId(jobId), jobPid(jobPid), cmd(cmd) {}
        int jobPid;
        Command *cmd;
        int jobId;
    };
  std::vector<JobEntry> jobs;
  public: 
    void removeFinishedJobs();
    void addJob(Command* cmd, int jobPid);
    void printJobsList();
    void killAllJobs();
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
};

class JobsCommand : public BuiltInCommand {
  public:
    JobsList* jobs;
    JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~JobsCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
  public:
    KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~KillCommand() {}
    void execute() override;

    JobsList* jobs;
};

class ForegroundCommand : public BuiltInCommand {
  public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~ForegroundCommand() {}
    void execute() override;

    JobsList* jobs;
};

class ChmodCommand : public BuiltInCommand {
  public:
    ChmodCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ChmodCommand() {}
    void execute() override;
};


class SmallShell {
  public:
    JobsList::JobEntry* fgJob = nullptr; 
    std::string promptName = "smash";
    JobsList jobs;
    int shellPid = getpid();
    SmallShell() {}
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
      static SmallShell instance; // Guaranteed to be destroyed.
      // Instantiated on first use.
      return instance;
    }
    void executeCommand(const char* cmd_line);
};

#endif //SMASH_COMMAND_H_

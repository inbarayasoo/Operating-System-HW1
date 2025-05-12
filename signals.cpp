#include "signals.h"
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include "Commands.h"
using namespace std;

void ctrlCHandler(int sig_num) {
    SmallShell& smash = SmallShell::getInstance();
    cout << "smash: got ctrl-C" << endl;
    JobsList::JobEntry* currJob = smash.fgJob;
    if (currJob == nullptr)
        return;
    if (kill(currJob->jobPid, SIGKILL) == -1)
        perror("smash error: kill failed");
    else
        cout << "smash: process " << currJob->jobPid << " was killed" << endl;
    currJob = nullptr;
}




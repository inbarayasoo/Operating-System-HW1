#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR)
        perror("smash error: failed to set ctrl-C handler");

    SmallShell& smash = SmallShell::getInstance();
    while (true) {
        std::cout << smash.promptName << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}

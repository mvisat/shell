#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "job.h"

using namespace std;

class Shell {
private:
    const int MAX_HISTORY;
    const int MAX_BUFFER;
    const string STRING_TILDE;
    bool exitNow;
    string ENV_HOME, ENV_PATH;
    vector<string> historyCommand;
    int historyIndex;
    struct termios old_termios, new_termios, shell_tmodes;
    void resetTermios();
    void initTermios();
    string readline();

    JobManager jobManager;

    pid_t shell_pgid;
    int shell_terminal, shell_is_interactive;

    static Shell* instance;
    static void SIGCHLD_HANDLER_STATIC(int);
    void SIGCHLD_HANDLER(int);

    void putJobForeground(Job&, bool);
    void putJobBackground(Job&, bool);
    void waitJob(Job&);
    void killJob(Job&);

public:
	Shell();
    ~Shell();

    vector<string> splitCommand(const string &, const char) const;
    char* trimCommand(char*);
    string trimCommand(string&);
	vector<string> parseCommand(const string&) const;
    void executeCommand(vector<string>&);
    void printPrompt();
    void runShell();
};

#endif

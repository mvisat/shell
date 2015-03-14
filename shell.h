#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>

#ifdef READLINE_LIBRARY
#  include "readline.h"
#  include "history.h"
#else
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

using namespace std;

class Shell {
private:
    const int MAX_BUFFER;
    const string STRING_TILDE;
    bool exitNow;
    string ENV_HOME, ENV_PATH;

public:
	Shell();

    vector<string> splitCommand(const string &, const char) const;
    char* trimCommand(char*);
	vector<string> parseCommand(const string&) const;
    void executeCommand(const vector<string>&);
    void runShell();
};

#endif

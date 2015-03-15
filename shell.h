#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    struct termios old_termios, new_termios;
    void resetTermios();
    void initTermios();
    int KeyPress();
    string readline();

public:
	Shell();
    ~Shell();

    vector<string> splitCommand(const string &, const char) const;
    char* trimCommand(char*);
    string trimCommand(string&);
	vector<string> parseCommand(const string&) const;
    void executeCommand(const vector<string>&);
    void runShell();


};

#endif

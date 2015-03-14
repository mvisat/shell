#include "shell.h"

using namespace std;


Shell::Shell():
    MAX_BUFFER(1024),
    STRING_TILDE("~") {
        exitNow = false;
        ENV_HOME = getenv("HOME");
        ENV_PATH = getenv("PATH");
}

vector<string> Shell::splitCommand(const string &s, const char delim) const {
    vector<string> elems;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim))
        elems.push_back(item);
    return elems;
}

char* Shell::trimCommand(char* cmdLine) {
    char *s, *t;

    for (s = cmdLine; whitespace (*s); ++s);

    if (*s == 0)
        return (s);

    t = s + strlen(s) - 1;
    while (t > s && whitespace (*t))
        --t;
    *++t = '\0';

    return s;
}

vector<string> Shell::parseCommand(const string& cmdLine) const {

    vector<string> vResult;
    if (cmdLine.size()) {
        const char
            CHAR_DOUBLE_QUOTE('"'),
            CHAR_SPACE(' '),
            CHAR_TAB('\t'),
            CHAR_SPACE_REPLACEMENT(1);
        bool isQuoted = false;
        string cmdResult = cmdLine;
        for (unsigned int i = 0; i < cmdResult.size(); ++i) {
            if (isQuoted) {
                switch (cmdResult[i]) {
                case CHAR_SPACE:
                case CHAR_TAB:
                    cmdResult[i] = CHAR_SPACE_REPLACEMENT;
                    break;
                case CHAR_DOUBLE_QUOTE:
                    isQuoted = false;
                    cmdResult[i] = CHAR_SPACE;
                    break;
                }
            }
            else {
                switch (cmdResult[i]) {
                case CHAR_DOUBLE_QUOTE:
                    isQuoted = true;
                case CHAR_TAB:
                    cmdResult[i] = CHAR_SPACE;
                    break;
                }
            }
        }
        vector<string> splitString = splitCommand(cmdResult, CHAR_SPACE);
        for (unsigned int i = 0; i < splitString.size(); ++i)
            if (splitString[i].size()) {
                for (unsigned int j = 0; j < splitString[i].size(); ++j)
                    if (splitString[i][j] == CHAR_SPACE_REPLACEMENT)
                        splitString[i][j] = CHAR_SPACE;
                vResult.push_back(splitString[i]);
            }
    }
    return vResult;
}

void Shell::executeCommand(const vector<string>& vCommand) {
    if (vCommand.size()) {
        // exit
        if (vCommand[0] == "exit")
            exitNow = true;
        // cd
        else if (vCommand[0] == "cd") {
            if (vCommand.size() > 1) {
                if (vCommand[1] == STRING_TILDE)
                    chdir(ENV_HOME.c_str());
                else if (chdir(vCommand[1].c_str()) < 0)
                    cerr << "cd: " << vCommand[1] << ": " << strerror(errno) << endl;
            }
            else
                chdir(ENV_HOME.c_str());
        }
        // TODO LIST
        /*
            -Built-in
            a. jobs: Mendaftarkan semua proses yang sedang berjalan melalui shell yang anda buat beserta PID nya.
            b. fg: Melanjutkan proses yang dihentikan dengan Ctrl+Z.
            c. kill: Menghentikan proses yang sedang berjalan.
            -Redirect STDIN dan STDOUT
            -Pipeline di antara 2 proses
        */
        // Selain built-in command
        else {
            int status;
            pid_t pid = fork();
            if (pid == 0) {
                // PID anak
                char** args;
                args = new char*[vCommand.size()+1];
                for (unsigned int i = 0; i < vCommand.size(); ++i) {
                    args[i] = new char[vCommand[i].size()+1];
                    for (unsigned int j = 0; j < vCommand[i].size(); ++j)
                        args[i][j] = vCommand[i][j];
                    args[i][vCommand[i].size()] = 0;
                }
                args[vCommand.size()] = 0;
                int execStatus = execvp(args[0], args);
                if (execStatus < 0)
                    cerr << args[0] << ": " << strerror(errno) << endl;
                delete [] args;
                exit(execStatus);
            }
            else {
                while (wait(&status) != pid);
            }
        }
    }
}

void Shell::runShell() {
    do {
        char currentDirectory[MAX_BUFFER];
        char cmdPrompt[MAX_BUFFER+2];

        // Shell menunjukkan lokasi dan direktori saat ini
		getcwd(currentDirectory, MAX_BUFFER);
        string currentDirectoryString = currentDirectory;
        if (currentDirectoryString.substr(0, ENV_HOME.size()) == ENV_HOME)
            currentDirectoryString.replace(0, ENV_HOME.size(), STRING_TILDE);
        sprintf(cmdPrompt, "%s$ ", currentDirectoryString.c_str());
        fflush(stdout);

        char *cmdLine = trimCommand(readline(cmdPrompt));
        if (cmdLine)
            add_history(cmdLine);

        string cmdLineString = cmdLine;
        free(cmdLine);

        executeCommand(parseCommand(cmdLineString));
	} while (!exitNow);
}

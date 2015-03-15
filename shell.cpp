#include "shell.h"

using namespace std;


Shell::Shell():
    MAX_BUFFER(1024),
    MAX_HISTORY(10),
    STRING_TILDE("~") {

    historyIndex = 0;
    exitNow = false;
    ENV_HOME = getenv("HOME");
    ENV_PATH = getenv("PATH");
    initTermios();
}

Shell::~Shell() {
    resetTermios();
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
    char* str = cmdLine;
    char *end;

    // Trim leading space
    while(isspace(*str)) str++;

    if(*str == 0)  // All spaces?
      return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;

    // Write new null terminator
    *(end+1) = 0;

    return str;
}

string Shell::trimCommand(string& cmdLine) {
    // trim trailing spaces
    string str = cmdLine;
    size_t endpos = str.find_last_not_of(" \t");
    if (string::npos != endpos)
        str = str.substr( 0, endpos+1 );

    // trim leading spaces
    size_t startpos = str.find_first_not_of(" \t");
    if( string::npos != startpos )
        str = str.substr(startpos);
    return str;
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
            bool pipelined = false;
            for (unsigned int i = 0; !pipelined && i < vCommand.size(); ++i)
                pipelined |= vCommand[i] == "|";

            if (pipelined) {
                vector<vector<string> > vCommandPipe;
                vector<string> vCommandArgv;
                for (unsigned int i = 0; i < vCommand.size(); ++i) {
                    if (vCommand[i] == "|") {
                        if (i == vCommand.size()-1) {
                            cerr << "pipelining: syntax error, expected command after '|'" << endl;
                            cerr.flush();
                            return;
                        }
                        else {
                            vCommandPipe.push_back(vCommandArgv);
                            vCommandArgv.clear();
                        }
                    }
                    else {
                        vCommandArgv.push_back(vCommand[i]);
                        if (i == vCommand.size()-1)
                            vCommandPipe.push_back(vCommandArgv);
                    }
                }

                vector<pid_t> vpid;
                int npipes = 2*(vCommandPipe.size()-1);
                int pipes[npipes];
                for (int i = 0; i < npipes; ++i)
                    if (pipe(pipes + 2*i) < 0) {
                        cerr << "pipelining: error, couldn't pipe" << endl;
                        cerr.flush();
                    }

                for (int i = 0; i < vCommandPipe.size(); ++i) {

                    pid_t pid = fork();
                    if (pid > 0)
                        vpid.push_back(pid);
                    else if (pid == 0) {
                        char** args;
                        args = new char*[vCommandPipe[i].size()+1];
                        for (unsigned int j = 0; j < vCommandPipe[i].size(); ++j) {
                            args[j] = new char[vCommandPipe[i].size()+1];
                            for (unsigned int k = 0; k < vCommandPipe[i][j].size(); ++k)
                                args[j][k] = vCommandPipe[i][j][k];
                            args[j][vCommandPipe[i][j].size()] = 0;
                        }
                        args[vCommandPipe[i].size()] = 0;

                        // Bukan command terakhir, bikin pipe STDOUT
                        if (i < vCommandPipe.size()-1) {
                            close(STDOUT_FILENO);
                            if (dup2(pipes[2*i+1], STDOUT_FILENO) < 0)
                                exit(EXIT_FAILURE);
                        }

                        // Bukan command pertama, bikin pipe STDIN
                        if (i) {
                            close(STDIN_FILENO);
                            if (dup2(pipes[2*(i-1)], STDIN_FILENO) < 0)
                                exit(EXIT_FAILURE);
                        }

                        for (int j = 0; j < npipes; ++j)
                            close(pipes[j]);

                        int execStatus = execvp(args[0], args);
                        if (execStatus < 0) {
                            cerr << args[0] << ": " << strerror(errno) << endl;
                            cerr.flush();
                        }
                        delete [] args;
                        exit(execStatus);
                    }
                    else {
                        cerr << "fork: failed to create child process" << endl;
                        return;
                    }
                }
                /* Main process nunggu anaknya mati :( */
                for (int i = 0; i < npipes; ++i)
                    close(pipes[i]);
                for (int i = 0 ; i < vpid.size(); ++i) {
                    int status;
                    waitpid(vpid[i], &status, 0);
                }
            }
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
        cout << currentDirectoryString << "$ ";
        cout.flush();

        string cmdPromptString;
        cmdPromptString = readline();
        cmdPromptString = trimCommand(cmdPromptString);
        if (cmdPromptString.size()) {
            if (historyCommand.size() == 0 || historyCommand[historyCommand.size()-1] != cmdPromptString)
                historyCommand.push_back(cmdPromptString);
            historyIndex = historyCommand.size();
        }

        executeCommand(parseCommand(cmdPromptString));
	} while (!exitNow);
}

static struct termios old_termios, new_termios;

/* restore new terminal i/o settings */
void Shell::resetTermios() {
    tcsetattr(0, TCSANOW,&old_termios);
}

/* initialize new terminal i/o settings */
void Shell::initTermios() {
    tcgetattr(0, &old_termios); // store old terminal
    new_termios = old_termios; // assign to new setting
    new_termios.c_lflag &= ~ICANON; // disable buffer i/o
    new_termios.c_lflag &= ~ECHO; // disable echo mode
    tcsetattr(0, TCSANOW, &new_termios); // use new terminal setting
}


/* Detect keyboard press */
int Shell::KeyPress() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

string Shell::readline() {
    int done = 0;
    string str = "";

    do {
        if (KeyPress()) {
            char cc = getchar();
            switch (cc) {
                case 27:
                    if ((cc = getchar()) == 91) {
                        bool historyChange = false;
                        switch (cc = getchar()) {
                        // User pencet atas
                        case 65:
                            if (historyCommand.size()) {
                                if (historyIndex > 0) {
                                    --historyIndex;
                                    historyChange = true;
                                }
                            }
                            break;
                        case 66:
                            if (historyCommand.size()) {
                                if (historyIndex < historyCommand.size()) {
                                    ++historyIndex;
                                    historyChange = true;
                                }
                            }
                            break;
                        }
                        if (historyChange) {
                            for (unsigned int i = 0; i < str.size(); ++i)
                                cout << "\b \b";

                            if (historyIndex >= 0 && historyIndex < historyCommand.size()) {
                                str = historyCommand[historyIndex];
                                cout << str;
                            }
                            else if (historyIndex == historyCommand.size()) {
                                str = "";
                                cout << str;
                            }
                        }
                    }
                    break;
                case '\n':
                    done = 1;
                    cout << endl;
                    break;
                case 127: // 127 = backspace
                    if (str.size()) {
                        cout << "\b \b";
                        str.erase(str.size()-1, 1);
                    }
                    break;
                default:
                    cout << cc;
                    str += cc;
            }
            cout.flush();
        }
    } while (!done);

    return str;
}

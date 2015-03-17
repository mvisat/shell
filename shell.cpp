#include "shell.h"

using namespace std;

Shell* Shell::instance;

Shell::Shell():
    MAX_BUFFER(1024),
    MAX_HISTORY(10),
    STRING_TILDE("~") {

    instance = this;
    historyIndex = 0;
    exitNow = false;
    ENV_HOME = getenv("HOME");
    ENV_PATH = getenv("PATH");

    /* See if we are running interactively.  */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty (shell_terminal);

    if (shell_is_interactive) {
        /* Loop until we are in the foreground.  */
        while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
            kill (-shell_pgid, SIGTTIN);

        /* Ignore interactive and job-control signals.  */
        signal (SIGINT, SIG_IGN);
        signal (SIGQUIT, SIG_IGN);
        signal (SIGTSTP, SIG_IGN);
        signal (SIGTTIN, SIG_IGN);
        signal (SIGTTOU, SIG_IGN);
        signal (SIGCHLD, &SIGCHLD_HANDLER_STATIC);

        /* Put ourselves in our own process group.  */
        shell_pgid = getpid ();
        if (setpgid (shell_pgid, shell_pgid) < 0) {
          cerr << "error: couldn't put the shell in its own process group" << endl;
          exit (1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp (shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr (shell_terminal, &shell_tmodes);

        initTermios();
    }
}

Shell::~Shell() {
    resetTermios();
}

void Shell::SIGCHLD_HANDLER_STATIC(int sig) {
    instance->SIGCHLD_HANDLER(sig);
}

void Shell::SIGCHLD_HANDLER(int sig) {
    pid_t pid;
    int terminationStatus;
    pid = waitpid(WAIT_ANY, &terminationStatus, WUNTRACED | WNOHANG);
    if (pid > 0) {
        Job job;
        if (!jobManager.Get(pid, job))
            return;
        if (WIFEXITED(terminationStatus)) {
            if (job.status == JobBackground) {
                cout << "[" << job.id+1 << "]+  Done\t   " << job.name << endl;
                jobManager.Delete(job);
            }
        }
        else if (WIFSIGNALED(terminationStatus)) {
            cout << "[" << job.id+1 << "]+  Killed\t   " << job.name << endl;
            jobManager.Delete(job);
        }
        else if (WIFSTOPPED(terminationStatus)) {
            if (job.status == JobBackground) {
                tcsetpgrp(shell_terminal, shell_pgid);
                jobManager.Change(pid, JobWaitingInput);
                cout << "[" << job.id+1 << "]+  Suspended\t   " << job.name << endl;
            }
            else {
                tcsetpgrp(shell_terminal, job.pgid);
                jobManager.Change(pid, JobSuspended);
                cout << "[" << job.id+1 << "]+  Stopped\t   " << job.name << endl;
            }
            return;
        }
        else {
            if (job.status == JobBackground)
                jobManager.Delete(job);
        }
        tcsetpgrp(shell_terminal, shell_pgid);
    }
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

void Shell::executeCommand(vector<string>& vCommand) {
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
            Ctrl+Z nya ga bisa T_T
        */
        // jobs
        else if (vCommand[0] == "jobs") {
            jobManager.Print();
        }
        // fg
        else if (vCommand[0] == "fg") {
            Job job;
            bool gotJob;
            if (vCommand.size() == 1)
                gotJob = jobManager.GetLastJob(job);
            else {
                stringstream ss(vCommand[1]);
                int jobId = -1;
                ss >> jobId;
                gotJob = jobManager.Get(job, jobId-1);
                if (!gotJob) {
                    cerr << "fg: " << jobId << ": no such job" << endl;
                }
            }
            if (!gotJob)
                return;
            if (job.status == JobSuspended || job.status == JobWaitingInput)
                putJobForeground(job, true);
            else
                putJobForeground(job, false);
        }
        // kill
        else if (vCommand[0] == "kill") {
            Job job;
            bool gotJob;
            if (vCommand.size() == 1)
                gotJob = jobManager.GetLastJob(job);
            else {
                bool byJobId = vCommand[1][0] == '%';
                if (byJobId)
                    vCommand[1].erase(0, 1);
                stringstream ss(vCommand[1]);
                int value = -1;
                ss >> value;
                if (byJobId)
                    gotJob = jobManager.Get(job, value-1);
                else
                    gotJob = jobManager.Get(value, job);
                if (!gotJob) {
                    if (byJobId)
                        cerr << "kill: " << value << ": no such job" << endl;
                    else
                        cerr << "kill: " << value << ": no such PID" << endl;
                }
            }
            if (!gotJob)
                return;
            killJob(job);
        }
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

                int npipes = 2*(vCommandPipe.size()-1);
                int pipes[npipes];
                for (int i = 0; i < npipes; ++i)
                    if (pipe(pipes + 2*i) < 0) {
                        cerr << "pipelining: error, couldn't pipe" << endl;
                        cerr.flush();
                    }

                vector<pid_t> vPID;
                for (int i = 0; i < vCommandPipe.size(); ++i) {

                    pid_t pid = fork();
                    if (pid == 0) {
                        resetTermios();

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
                    else if (pid > 0)
                        vPID.push_back(pid);
                    else {
                        cerr << "fork: failed to create child process" << endl;
                        return;
                    }
                }

                /* Main process nunggu anaknya mati :( */
                for (int i = 0; i < npipes; ++i)
                    close(pipes[i]);
                for (int i = 0 ; i < vPID.size(); ++i) {
                    int status;
                    waitpid(vPID[i], &status, 0);
                }
            }
            else {
                // Background process
                bool background = vCommand[vCommand.size()-1] == "&";
                if (background) {
                    vCommand.erase(vCommand.begin()+vCommand.size()-1);
                }
                else {

                }

                int status;
                pid_t pid = fork();
                if (pid == 0) {
                    resetTermios();

                    // STDIN, diubah sesuai yang ada di reference
                    int fileInput = -1;
                    for (unsigned int i = 0; fileInput == -1 && i < vCommand.size(); ++i) {
                        if (vCommand[i] == "<") {
                            if (i == vCommand.size()-1) {
                                cerr << "redirect stdin: syntax error, expected filename after '<'" << endl;
                                return;
                            }
                            else {
                                fileInput = open(vCommand[i+1].c_str(), O_RDONLY);
                                if (fileInput == -1) {
                                    cerr << "redirect stdin: couldn't open the file '" << vCommand[i+1] << "'" <<  endl;
                                    return;
                                }
                                vCommand.erase(vCommand.begin()+i);
                                vCommand.erase(vCommand.begin()+i);
                            }
                        }
                    }

                    // STDOUT, diubah sesuai yang ada di reference
                    int fileOutput = -1;
                    for (unsigned int i = 0; fileOutput == -1 && i < vCommand.size(); ++i) {
                        if (vCommand[i] == ">") {
                            if (i == vCommand.size()-1) {
                                cerr << "redirect stdout: syntax error, expected filename after '>'" << endl;
                                return;
                            }
                            else {
                                fileOutput = open(vCommand[i+1].c_str(), O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
                                if (fileOutput == -1) {
                                    cerr << "redirect stdout: couldn't open the file '" << vCommand[i+1] << "'" << endl;
                                    return;
                                }
                                vCommand.erase(vCommand.begin()+i);
                                vCommand.erase(vCommand.begin()+i);
                            }
                        }
                    }

                    setpgrp();
                    if (background)
                        cout << endl << "[" << jobManager.GetActiveJobs() << "] " << getpid() << endl;
                    else
                        tcsetpgrp(shell_terminal, pid);

                    /* Set the handling for job control signals back to the default.  */
                    signal (SIGINT, SIG_DFL);
                    signal (SIGQUIT, SIG_DFL);
                    signal (SIGTSTP, SIG_DFL);
                    signal (SIGTTIN, SIG_DFL);
                    signal (SIGTTOU, SIG_DFL);
                    signal (SIGCHLD, &SIGCHLD_HANDLER_STATIC);

                    char** args;
                    args = new char*[vCommand.size()+1];
                    for (unsigned int i = 0; i < vCommand.size(); ++i) {
                        args[i] = new char[vCommand[i].size()+1];
                        for (unsigned int j = 0; j < vCommand[i].size(); ++j)
                            args[i][j] = vCommand[i][j];
                        args[i][vCommand[i].size()] = 0;
                    }
                    args[vCommand.size()] = 0;

                    if (fileInput != -1) {
                        close(STDIN_FILENO);
                        dup(fileInput);
                    }
                    if (fileOutput != -1) {
                        close(STDOUT_FILENO);
                        dup(fileOutput);
                    }

                    int execStatus = execvp(args[0], args);
                    if (execStatus < 0)
                        cerr << args[0] << ": " << strerror(errno) << endl;

                    // Cleanup
                    delete [] args;
                    if (fileInput != -1)
                        close(fileInput);
                    if (fileOutput != -1)
                        close(fileOutput);

                    exit(execStatus);
                }
                else if (pid > 0) {
                    setpgid(pid, pid);
                    Job job = jobManager.Insert(pid, pid, vCommand[0], background ? JobBackground : JobForeground);
                    if (background)
                        putJobBackground(job, false);
                    else
                        putJobForeground(job, false);
                }
                else {
                    cerr << "fork: failed to create child process" << endl;
                }
            }
        }
    }
}

void Shell::printPrompt() {
    char currentDirectory[MAX_BUFFER];

    // Shell menunjukkan lokasi dan direktori saat ini
    getcwd(currentDirectory, MAX_BUFFER);
    string currentDirectoryString = currentDirectory;
    if (currentDirectoryString.substr(0, ENV_HOME.size()) == ENV_HOME)
        currentDirectoryString.replace(0, ENV_HOME.size(), STRING_TILDE);
    cout << currentDirectoryString << "$ ";
    cout.flush();
}

void Shell::runShell() {
    do {
        printPrompt();

        string cmdPromptString;
        cmdPromptString = readline();
        cmdPromptString = trimCommand(cmdPromptString);
        if (cmdPromptString.size()) {
            if (historyCommand.size() == 0 || historyCommand[historyCommand.size()-1] != cmdPromptString)
                historyCommand.push_back(cmdPromptString);
            historyIndex = historyCommand.size();
        }
        vector<string> vcmdPromptString = parseCommand(cmdPromptString);
        executeCommand(vcmdPromptString);
	} while (!exitNow);
}

static struct termios old_termios, new_termios;

/* restore new terminal i/o settings */
void Shell::resetTermios() {
    tcsetattr(0, TCSANOW, &shell_tmodes);
}

/* initialize new terminal i/o settings */
void Shell::initTermios() {
    new_termios = shell_tmodes; // assign to new setting
    new_termios.c_lflag &= ~ICANON; // disable buffer i/o
    new_termios.c_lflag &= ~ECHO; // disable echo mode
    tcsetattr(0, TCSANOW, &new_termios); // use new terminal setting
}

string Shell::readline() {
    int done = 0;
    string str = "";

    do {
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
                    // User pencet bawah
                    case 66:
                        if (historyCommand.size()) {
                            if (historyIndex < historyCommand.size()) {
                                ++historyIndex;
                                historyChange = true;
                            }
                        }
                        break;
                    // Pencet kiri sama kanan bisa tapi susah implementasinya
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
    } while (!done);

    return str;
}

void Shell::putJobForeground(Job& job, bool continueJob) {
    job.status = JobForeground;
    tcsetpgrp(shell_terminal, job.pgid);
    if (continueJob) {
        if (kill(-job.pgid, SIGCONT) < 0)
            cerr << "error: kill SIGCONT" << endl;
    }
    waitJob(job);

    /* Put the shell back in the foreground.  */
    tcsetpgrp (shell_terminal, shell_pgid);

    initTermios();
}

void Shell::putJobBackground(Job& job, bool continueJob) {
    if (continueJob && job.status != JobWaitingInput)
        job.status = JobWaitingInput;
    if (continueJob)
        if (kill(-job.pgid, SIGCONT) < 0)
            cerr << "error: kill SIGCONT" << endl;

    tcsetpgrp(shell_terminal, shell_pgid);
}


void Shell::waitJob(Job& job) {
    int terminationStatus;
    while (waitpid(job.pid, &terminationStatus, WUNTRACED | WNOHANG) == 0) {
        jobManager.Get(job.pid, job);
        if (job.status == JobSuspended) {
            return;
        }
    }
    jobManager.Delete(job);
}

void Shell::killJob(Job& job) {
    kill(job.pid, SIGKILL);
}

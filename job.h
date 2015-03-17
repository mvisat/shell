#ifndef JOB_H
#define JOB_H

#include <vector>

#include <iostream>
#include <cstdio>

#include <unistd.h>

using namespace std;

struct Job {
    int id;
    string name;
    pid_t pid;
    pid_t pgid;
    int status;
};

enum JobStatus {
    JobBackground,
    JobForeground,
    JobWaitingInput,
    JobSuspended
};

class JobManager {
private:
    vector<Job> jobsList;
    int nActiveJobs;

public:
    JobManager();
    ~JobManager();

    bool Get(JobStatus, Job&);
    bool Get(pid_t, Job&);
    bool Get(Job&, int);
    bool GetLastJob(Job&);
    Job Insert(pid_t, pid_t, string&, JobStatus);
    bool Change(pid_t, JobStatus);
    void Delete(Job&);
    void Print();

    int GetActiveJobs() { return jobsList.size(); };
};

#endif

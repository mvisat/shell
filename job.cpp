#include "job.h"

using namespace std;

JobManager::JobManager() {

}

JobManager::~JobManager() {

}

Job JobManager::Insert(pid_t pid, pid_t pgid, string& name, JobStatus status) {
    Job newJob;

    newJob.id = jobsList.size();
    newJob.name = name;
    newJob.pid = pid;
    newJob.pgid = pgid;
    newJob.status = status;

    jobsList.push_back(newJob);
    return newJob;
}

bool JobManager::Change(pid_t pid, JobStatus status) {
    for (unsigned int i = 0; i < jobsList.size(); ++i) {
        if (jobsList[i].pid == pid) {
            jobsList[i].status = status;
            return true;
        }
    }
    return false;
}

void JobManager::Delete(Job& job) {
    for (unsigned int i = 0; i < jobsList.size(); ++i) {
        if (jobsList[i].pid == job.pid) {
            jobsList.erase(jobsList.begin() + i);
            return;
        }
    }
}

bool JobManager::Get(pid_t searchValue, Job& job) {
    for (unsigned int i = 0; i < jobsList.size(); ++i)
        if (searchValue == jobsList[i].pid) {
            job = jobsList[i];
            return true;
        }
    return false;
}

bool JobManager::Get(Job& job, int searchValue) {
    for (unsigned int i = 0; i < jobsList.size(); ++i)
        if (searchValue == jobsList[i].id) {
            job = jobsList[i];
            return true;
        }
    return false;
}

bool JobManager::Get(JobStatus searchValue, Job& job) {
    for (unsigned int i = 0; i < jobsList.size(); ++i)
        if (searchValue == jobsList[i].status) {
            job = jobsList[i];
            return true;
        }
    return false;
}

bool JobManager::GetLastJob(Job& job) {
    if (jobsList.size()) {
        job = jobsList[jobsList.size()-1];
        return true;
    }
    return false;
}

void JobManager::Print()
{
        cout << "# Active Jobs: " << jobsList.size() << endl;
        for (unsigned int i = 0; i < jobsList.size(); ++i) {
            string status = "";
            switch (jobsList[i].status) {
            case JobForeground:
                status = "Foreground";
                break;
            case JobBackground:
                status = "Background";
                break;
            case JobSuspended:
                status = "Suspended";
                break;
            case JobWaitingInput:
                status = "Waiting Input";
                break;
            }
            cout \
            << "[" << jobsList[i].id+1 << "] " \
            << jobsList[i].pid << ", " \
            << jobsList[i].name << ", " \
            << status \
            << endl;
            //printf("|  %7d | %30s | %5d | %10s | %6c |\n", jobsList[i].id, jobsList[i].name, jobsList[i].pid, jobsList[i].descriptor, jobsList[i].status);
        }
}

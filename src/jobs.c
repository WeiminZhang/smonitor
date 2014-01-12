#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <sched.h>

#include "smonitor.h"
#include "jobs.h"

static int set_priority(pid_t pid, int32_t priority) {
    struct sched_param sp;
    sp.sched_priority = priority;
    return sched_setscheduler(pid, SCHED_RR, &sp);
}

static void sort_pids(pid_t *pids, size_t count) {
    size_t i, j;
    pid_t value;
    for(i = 1; i < count; i++) {
        value = pids[i];
        for(j = i; j > 0 && value < pids[j-1]; j--) {
            pids[j] = pids[j-1];
        }
        pids[j] = value;
    }
}

static bool pid_is_zombie(pid_t pid) {
    FILE *fh = NULL;
    char buf[200] = {0};
    char status;
    bool result = false;

    snprintf(buf, 200, "/proc/%d/stat", pid);

    fh = fopen(buf, "r");
    if (fh == NULL) {
        result = true;
        goto cleanup;
    }

    if (fscanf(fh, "%*d %*s %c", &status) != 1) {
        result = true;
        goto cleanup;
    }
    
    if (status == 'Z') {
        result = true;
    }

    cleanup: {
        if (fh != NULL) {
            fclose(fh);
        }
        return result;
    }

}

static int check_pids(job_t *job, pid_t *pids, size_t count) {
    size_t i, j;
    bool found = false;

    if (count < job->pids_count) {
        return J_RESPAWN;
    }

    for (i = 0; i < job->pids_count; i++) {
        if (pid_is_zombie(job->pids[i])) {
            printf("[%s] zombie with %d pid found\n", job->config.name, job->pids[i]);
            return J_RESPAWN;
        }

        found = false;
        for (j = 0; i < count; j++) {
            if (job->pids[i] == pids[j]) {
                found = true;
                break;
            }
        }

        if (!found) {
            printf("[%s] child pid %d not found\n", job->config.name, job->pids[i]);
            return J_RESPAWN;
        }
    }

    return J_NOACTION;
}

static size_t get_userpids_by_command(char* user, char *command, pid_t *pids, size_t max_num) {
    FILE* ph;
    size_t count;
    char buf[500];
    char commandbuf[100];
    pid_t pid;

    snprintf(buf, 500, "ps -u %s -o \"%%p %%a\"", user);

    ph = popen(buf, "r");
    if (ph == NULL) {
        fprintf(stderr, "Function popen(%s) failed: %s\n", buf, strerror(errno));
        return 0;
    }

    count = 0;
    while ( fgets(buf, 500, ph) != NULL) {
        if (count >= max_num) {
            break;
        }

        memset(commandbuf, 0, 100);
        if (sscanf(buf, "%d %99c", &pid, commandbuf) != 2) {
            continue;
        }
        if (strstr(commandbuf, command)) {
            pids[count] = pid;
            count++;
        }
    }
    pclose(ph);

    sort_pids(pids, count);

    return count;
}

static int set_uidgid(char *username) {
    struct passwd *pwdfile = NULL;

    pwdfile = getpwnam(username);
    if (pwdfile == NULL) {
        fprintf(stderr, "Function getpwnam() failed: %s\n", strerror(errno));
        return -1;
    }

    if (setgid(pwdfile->pw_gid) == -1) {
        fprintf(stderr, "Function setgid() failed: %s\n", strerror(errno));
        return -1;
    }

    if (setuid(pwdfile->pw_uid) == -1) {
        fprintf(stderr, "Function setuid() failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}


static int track_children_job(job_t *job) {
    pid_t pids[MAX_PIDS_NUM];
    size_t pids_count = 0;
    int retcode;
    struct passwd *pwdfile = NULL;
    char *username = NULL;

    if (*(job->config.user)) {
        username = job->config.user;
    } else {
        pwdfile = getpwuid(getuid());
        if (pwdfile == NULL) {
            perror("Getting username of current uid");
            return J_NOACTION;
        }
        username = pwdfile->pw_name;
    }

    //printf("[%s] getting pids for %s user\n", job->config.name, username);

    pids_count = get_userpids_by_command(username, job->config.name, pids, MAX_PIDS_NUM);

    if (job->pids_count == 0) {
        // first time. just save
        memcpy((uint8_t*)job->pids, (uint8_t*)pids, MAX_PIDS_NUM * sizeof(pid_t));
        job->pids_count = pids_count;
        printf("[%s] pids are saved: %zu\n", job->config.name, pids_count);
        retcode =  J_NOACTION;
    } else {
        retcode = check_pids(job, pids, pids_count);
    }
    return retcode;
}

int start_job(job_t *job)
{
    pid_t    pid;
    char*    in_filename = NULL;
    char*    out_filename = NULL;
    int      in_fd; 
    int      out_fd;
    int      retcode = -1; 
    uint8_t  need_delay = 0;
    char*    argv[MAX_COMMAND_ARGUMENTS] = {};
    int      i;


    if ( job == NULL) {
        return -1;
    }

    job->start_time  = time(NULL) + job->config.delay_start;
    job->pids_count = 0;
    memset(job->pids, 0, MAX_PIDS_NUM);

    // first run?
    if (job->pid == 0) {
        need_delay = 1;
        printf("[%s] will be executed with delay %u\n", job->config.name, job->config.delay_start);
    }

    pid = fork();

    if (pid == (pid_t)-1) {
        fprintf(stderr, "[%s] Can't fork process, error: %s\n", job->config.name, strerror(errno));
        return -2;
    }

    job->pid = pid;

    if (pid > (pid_t)0) {
        return 0;
    }

    prctl(PR_SET_PDEATHSIG, SIGHUP);

    if (job->config.priority) {
        retcode = set_priority(pid, job->config.priority);
        if (retcode != 0) {
            printf("[%s] Warning: setting priority failed\n", job->config.name);
        }
    }

    if (*(job->config.user)) {
        retcode = set_uidgid(job->config.user);
        if (retcode == -1) {
            printf("[%s] Warning: changing uid or gid failed\n", job->config.name);
        }
    }

    if ( *(job->config.workdir) && (chdir(job->config.workdir) == -1) ) {
        printf("[%s] Changing directory failed: %s\n", job->config.name, strerror(errno));
        _exit(-1);
    }

    in_filename  = *(job->config.in_file)  ? job->config.in_file  : "/dev/null";
    out_filename = *(job->config.out_file) ? job->config.out_file : "/dev/null";

    in_fd = open(in_filename, O_RDONLY);
    if (in_fd == -1) {
        printf("[%s] Open %s failed: %s\n", job->config.name, in_filename, strerror(errno));
        _exit(-1);
    }

    out_fd = open(out_filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (out_fd == -1) {
        printf("[%s] Open %s failed: %s\n", job->config.name, out_filename, strerror(errno));
        close(in_fd);
        _exit(-1);
    }


    if ( dup2(in_fd, STDIN_FILENO) == -1 ) {
        close(in_fd);
        close(out_fd);
        _exit(-1);
    }

    if ( dup2(out_fd, STDOUT_FILENO) == -1 ) {
        close(in_fd);
        close(out_fd);
        _exit(-1);
    }

    if ( dup2 (STDOUT_FILENO, STDERR_FILENO) < 0 ) {
        close(in_fd);
        close(out_fd);
        _exit(-1);
    }


    if (need_delay == 1) {
        sleep(job->config.delay_start);
    }

    for(i = 0; i < job->config.cmd_len; i++) {
        argv[i] = job->config.cmd[i];
    }

    if (execv(argv[0], argv)) {
        perror("execv() failed");
        _exit(-1);
    }

    return 0;
}

int start_jobs(job_t *jobs, uint32_t jobs_num) {
    size_t i;
    int retcode;

    for (i = 0; i < jobs_num; i++) {
        retcode = start_job(&jobs[i]);
        if ( retcode < 0) {
            printf("Error: %s - creating subprocess failed\n", jobs[i].config.name);
            return -1;
        }
        printf("[%s] forked, pid = %d\n", jobs[i].config.name, jobs[i].pid);
    }
    return 0;
}

int wait_job(job_t *job) {
    int status;
    pid_t pid;

    pid = waitpid(job->pid, &status, WNOHANG);
    if (pid == 0) {
        return J_NOACTION;
    }

    if (pid == (pid_t)-1) {
        printf("[%s] Error with wait(): %s\n", job->config.name, strerror(errno));
        return J_ERROR;
    }

    return J_RESPAWN;
}

int deep_track(job_t *job)
{
    int retcode;

    if (job->config.track_children) {
        retcode = track_children_job(job);
        if (retcode == J_RESPAWN) {
            return J_RESPAWN;
        }
    }

    // if (job->config.track_memory) {
    // And other tracking should be here
    // }

    return J_NOACTION;
}


void greaceful_kill(job_t *job, int sig) {
    size_t i;
    bool parent_killed = false;

    for(i = 0; i < job->pids_count; i++) {
        kill(job->pids[i], sig);
        if (job->pids[i] == job->pid) {
            parent_killed = true;
        }
    }

    if (!parent_killed) {
        kill(job->pid, sig);
    }
}
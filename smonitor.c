#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>

#define SMONITOR_INTERVAL   500000

#include "jobs.h"

#define log_msg(...) printf(__VA_ARGS__)

int wait_child(job_t *job) {
    int status;
    pid_t pid;

    pid = waitpid(job->pid, &status, WNOHANG);
    if (pid == 0) {
        log_msg("[Job] '%s' is running\n", job->config.name);
        return J_NOACTION;
    } else {

    }

    if (pid == (pid_t)-1) {
        log_msg("[Job] ERROR with %s job: %s\n", job->config.name, strerror(errno));
        return J_ERROR;
    }

    if (job->config.once) {
        log_msg("[Job] '%s' finished\n", job->config.name);
        job->finished = 1;
        return J_NOACTION;
    }

    return J_RESPAWN;
}

int track_child(job_t *job)
{
    FILE*    fh;
    char     buf[1024];
    uint32_t counter = 0;

    if (job->config.track_children) {

        snprintf(buf, 1024, "pstree -a %d", job->pid);

        fh = popen(buf, "r");
        if (fh == NULL) {
            log_msg("Function popen(%s) failed: %s\n", buf, strerror(errno));
            return J_NOACTION;
        }

        while ( fgets(buf, 1024, fh) != NULL) {
             counter++;
        }

        pclose(fh);

        if (job->child_count == 0) {
            job->child_count = counter;
        } else {
            if(counter < job->child_count) {
                log_msg("[Job] seems like some child of '%s' is dead\n", job->config.name);
                return J_RESPAWN;
            }
        }

    }

    return J_NOACTION;
}


int find_job_by_pid(job_t *jobs, size_t count, pid_t pid)
{
    uint32_t i = 0;
    for (i = 0; i < count; i++) {
        if (jobs[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int set_uidgid(char *username) {
    struct passwd *pwdfile = NULL;

    pwdfile = getpwnam(username);
    if (pwdfile == NULL) {
        log_msg("Function getpwnam() failed: %s\n", strerror(errno));
        return -1;
    }

    if (setgid(pwdfile->pw_gid) == -1) {
        log_msg("Function setgid() failed: %s\n", strerror(errno));
        return -1;
    }

    if (setuid(pwdfile->pw_uid) == -1) {
        log_msg("Function setuid() failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
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

    // check if it's started yet
    if (job->pid == 0) {
        need_delay = 1;
        log_msg("[Job] '%s' will be executed with delay = %u\n", job->config.name, job->config.delay_start);
    }

    pid = fork();

    if (pid == (pid_t)-1) {
        log_msg("Can't fork process, error: %s\n", strerror(errno));
        return -2;
    }

    job->pid = pid;

    if (pid > (pid_t)0) {
        job->start_time  = time(NULL);
        job->child_count = 0;
        return 0;
    }

    prctl(PR_SET_PDEATHSIG, SIGHUP);

    if (*job->config.user) {
        retcode = set_uidgid(job->config.user);
        if (retcode == -1) {
            log_msg("Warning: changing uid or guid failed\n");
        }
    }

    if ( *job->config.workdir && (chdir(job->config.workdir) == -1) ) {
        log_msg("Changing directory failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    in_filename  = *job->config.in_file  ? job->config.in_file  : "/dev/null";
    out_filename = *job->config.out_file ? job->config.out_file : "/dev/null";

    in_fd = open(in_filename, O_RDONLY);
    if (in_fd == -1) {
        log_msg("Open %s failed: %s\n", in_filename, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    out_fd = open(out_filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (out_fd == -1) {
        log_msg("Open %s failed: %s\n", out_filename, strerror(errno));
        close(in_fd);
        _exit(EXIT_FAILURE);
    }


    if ( dup2(in_fd, STDIN_FILENO) == -1 ) {
        close(in_fd);
        close(out_fd);
        _exit(EXIT_FAILURE);
    }

    if ( dup2(out_fd, STDOUT_FILENO) == -1 ) {
        close(in_fd);
        close(out_fd);
        _exit(EXIT_FAILURE);
    }

    if ( dup2 (STDOUT_FILENO, STDERR_FILENO) < 0 ) {
        close(in_fd);
        close(out_fd);
        _exit(EXIT_FAILURE);
    }


    if (need_delay == 1) {
        sleep(job->config.delay_start);
    }

    for(i = 0; i < job->config.cmd_len; i++) {
        argv[i] = job->config.cmd[i];
    }

    if (execv(argv[0], argv)) {
        perror("execv() failed");
        _exit(EXIT_FAILURE);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    job_t*   jobs       = NULL;
    job_t*   curr_job   = NULL;
    size_t   jobs_count = 0;
    time_t   track_time = 0;
    time_t   now        = 0;
    int32_t  retcode    = 0;
    size_t i           = 0;
    char* filename_config_json = NULL;

    if ( argc < 2 ) {
        log_msg("No config file specified\n");
        exit(EXIT_FAILURE);
    }

    filename_config_json = argv[1];


    if ( get_jobs_json(&jobs, &jobs_count, filename_config_json) == -1) {
        log_msg("Error: getting jobs failed\n");
        exit(EXIT_FAILURE);
    }
    
    for (i = 0; i < jobs_count; i++) {
        retcode = start_job(&jobs[i]);
        if ( retcode < 0) {
            log_msg("Error: %s - creating subprocess failed\n", jobs[i].config.name);
            exit(EXIT_FAILURE);
        }
        log_msg("Process %s forked, pid = %d\n", jobs[i].config.name, jobs[i].pid);
    }

    while (1) { 
        usleep(SMONITOR_INTERVAL);

        for (i = 0; i < jobs_count; i++) {
            curr_job = &jobs[i];

            if (curr_job->finished > 0) {
                continue;
            }

            retcode = wait_child(curr_job);
            if (retcode == J_RESPAWN) {
                log_msg("[Job] '%s' needs to be respawned\n", curr_job->config.name);
                start_job(curr_job);
                continue;
            }

            track_time = curr_job->start_time + curr_job->config.delay_track;
            now = time(NULL);
            if (now < track_time) {
                log_msg("Too small time to track_time\n");
                continue;
            }

            retcode = track_child(curr_job);
            if (retcode == J_RESPAWN) {
                log_msg("[Job] '%s' needs to be killed\n", curr_job->config.name);
                kill(curr_job->pid, SIGKILL);
                continue;
            }

        }
        log_msg("\n");
    }

    return 0;
}

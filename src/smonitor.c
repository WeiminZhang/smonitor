#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <time.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <sched.h>


#include "smonitor.h"
#include "config.h"
#include "jobs.h"


static void tracking_loop(smonitor_t *monitor) {
    int retcode;
    size_t i;
    time_t track_time = 0;
    time_t now        = 0;
    job_t *job        = NULL;

    while (true) {
        usleep(monitor->interval);
        now = time(NULL);

        for (i = 0; i < monitor->jobs_num; i++) {
            job = &( monitor->jobs[i] );

            if (job->finished > 0) {
                continue;
            }

            retcode = wait_job(job);
            if (retcode == J_NOACTION) {
                printf("[%s] running\n", job->config.name);
            } else {
               
                if (job->config.once) {
                    printf("[%s] finished execution\n", job->config.name);
                    job->finished = true;
                    continue;
                }

                printf("[%s] terminated. Restarting\n", job->config.name);
                start_job(job);
            }


            track_time = job->start_time + job->config.delay_track;
            if (now < track_time) {
                printf("[%s] skipping deep tracking\n", job->config.name);
                continue;
            }

            retcode = deep_track(job);
            if (retcode == J_RESPAWN) {
                printf("[%s] needs to be killed\n", job->config.name);
                greaceful_kill(job, SIGTERM);
                continue;
            }
        }
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    int32_t  retcode              = 0;
    char* filename_config_json    = NULL;

    // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119
    smonitor_t monitor;
    memset(&monitor,0, sizeof(smonitor_t));

    if ( argc < 2 ) {
        printf("No config file specified\n");
        exit(EXIT_FAILURE);
    }

    filename_config_json = argv[1];

    retcode = parse_config(filename_config_json, &monitor);
    if ( retcode != 0) {
        printf("Error: getting jobs failed\n");
        exit(EXIT_FAILURE);
    }

    retcode = start_jobs(monitor.jobs, monitor.jobs_num);
    if ( retcode < 0) {
        exit(EXIT_FAILURE);
    }

    tracking_loop(&monitor);

    return 0;
}

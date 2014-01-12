/**
 *******************************************************************************
 * Macros for actions when job exists
 ******************************************************************************/
#define J_NOACTION  0
#define J_RESPAWN   1
#define J_ERROR     127

int start_job(job_t *job);
int start_jobs(job_t *jobs, uint32_t jobs_num);
int wait_job(job_t *job);
int deep_track(job_t *job);
void greaceful_kill(job_t *job, int sig);
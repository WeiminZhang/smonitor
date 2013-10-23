#include <inttypes.h>
#include <sys/types.h>


/**
 *******************************************************************************
 * Max number of jobs
 ******************************************************************************/
#define MAX_JOBS_NUM 16

/**
 *******************************************************************************
 * Max value for command line argument which will be run
 ******************************************************************************/
#define MAX_COMMAND_ARGUMENTS 10

/**
 *******************************************************************************
 * Buffer size. It's used when reading files
 ******************************************************************************/
#define BUFFER_SIZE 8192

 /**
 *******************************************************************************
 * Macros for actions when job exists
 ******************************************************************************/
#define J_NOACTION  0
#define J_RESPAWN   1
#define J_ERROR     127


 /**
 *******************************************************************************
 * Max length of string in fields of config 
 ******************************************************************************/
#define MAX_FIELD_LEN 100

/**
 *******************************************************************************
 * A structure to represent a config for jobs.
 * Configuration info about cmd, workdir, priority, etc.
 ******************************************************************************/
typedef struct {
    char     name     [MAX_FIELD_LEN];
    char     cmd      [MAX_COMMAND_ARGUMENTS][MAX_FIELD_LEN];
    uint8_t  cmd_len;
    char     workdir  [MAX_FIELD_LEN];
    char     out_file [MAX_FIELD_LEN];
    char     in_file  [MAX_FIELD_LEN];
    char     user     [MAX_FIELD_LEN];
    uint8_t   delay_start;
    uint8_t   delay_track;
    uint8_t   track_children;
    uint8_t   once;
} config_t;

/**
 *******************************************************************************
 * A structure to represent a job.
 * Job is a process which will be executed and tracked.
 ******************************************************************************/
typedef struct {
    pid_t    pid;
    time_t   start_time;
    uint32_t child_count;
    uint8_t  finished;
    uint8_t  running;
    config_t config;
} job_t; 

/**
 *******************************************************************************
 * Gets array of jobs
 * @param[out] jobs - address of pointer where the first job_t will be located
 * @param[out] count - addres of size_t variable for number of jobs
 * @return -1 if errror occured, otherwise 0
 ******************************************************************************/
int get_jobs(job_t **jobs, size_t *count);

/**
 *******************************************************************************
 * Gets array of jobs using json config file
 * @param[out] jobs - address of pointer where the first job_t will be located
 * @param[out] count - addres of size_t variable for number of jobs
 * @param[in] filename - path to json config file
 * @return -1 if errror occured, otherwise 0
 ******************************************************************************/
int get_jobs_json(job_t **jobs, size_t *count, char const *filename);
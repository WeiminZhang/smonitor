/**
 *******************************************************************************
 * Max number of jobs
 ******************************************************************************/
#define MAX_JOBS_NUM 18

/**
 *******************************************************************************
 * Max value for command line argument which will be run
 ******************************************************************************/
#define MAX_COMMAND_ARGUMENTS 10

/**
 *******************************************************************************
 * Max length of string in fields of config 
 ******************************************************************************/
#define MAX_FIELD_LEN 100

/**
 *******************************************************************************
 * Buffer size. It's used when reading files
 ******************************************************************************/
#define BUFFER_SIZE 8192

/**
 *******************************************************************************
 * Default interval for checking children in ms
 ******************************************************************************/
#define DEFAULT_INTERVAL 500000

/**
 *******************************************************************************
 * Max number of children for each process
 ******************************************************************************/
#define MAX_PIDS_NUM 20

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
    int32_t   priority;
    uint8_t   delay_start;
    uint8_t   delay_track;
    bool      once;
    bool      track_children; 
} config_t;

/**
 *******************************************************************************
 * A structure to represent a job.
 * Job is a process which will be executed and tracked.
 ******************************************************************************/
typedef struct {
    pid_t    pid;
    pid_t    pids[MAX_PIDS_NUM];
    uint32_t pids_count;
    uint8_t  finished;
    time_t   start_time;
    config_t config;
} job_t;


/**
 *******************************************************************************
 * A structure to represent internal smonitor state.
 ******************************************************************************/
typedef struct {
    char     smonitor_cflag[MAX_FIELD_LEN];
    uint64_t interval;
    job_t    jobs[MAX_JOBS_NUM];
    uint32_t jobs_num;
} smonitor_t;
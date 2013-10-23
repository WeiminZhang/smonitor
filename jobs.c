#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>

static job_t s_jobs[MAX_JOBS_NUM];

static int read_text_file(char const *filename, char* buffer, size_t len_max)
{
    int retcode = 0;
    size_t elements_read = 0;
    FILE* fh = NULL;

    memset(buffer, 0, len_max);
    
    fh = fopen(filename, "r");
    if ( fh == NULL ) {
        return -1;
    }

    elements_read = fread(buffer, 1, len_max, fh);
    if ( elements_read == 0 ) {
        retcode = -1;
    }

    buffer[elements_read] = 0; // FIO17-C

    if ( fh != NULL) {
        fclose(fh);
    }

    return retcode;
}


int complete_config_from_json(config_t *config, JsonNode *node) {
    JsonNode *field = NULL;
    JsonNode *c;
    int i;

    memset((uint8_t*)config, 0, sizeof(config_t));


    field = json_find_member(node, "name");
    if (field == NULL || field->tag != JSON_STRING) {
        return -1;
    }
    strncpy(config->name, field->string_, MAX_FIELD_LEN);

    field = json_find_member(node, "cmd");
    if (field == NULL || field->tag != JSON_ARRAY) {
        return -1;
    }
    i = 0;
    json_foreach(c, field) {
        if (i >= MAX_COMMAND_ARGUMENTS) {
            break;
        }
        if (c->tag != JSON_STRING) {
            return -1;
        }
        strncpy(config->cmd[i], c->string_, MAX_FIELD_LEN);
        i++;
    }
    config->cmd_len = i;

    field = json_find_member(node, "workdir");
    if (field != NULL) {
        strncpy(config->workdir, field->string_, MAX_FIELD_LEN);
    }
    
    field = json_find_member(node, "out_file");
    if (field != NULL) {
        strncpy(config->out_file, field->string_, MAX_FIELD_LEN);
    }

    field = json_find_member(node, "in_file");
    if (field != NULL) {
        strncpy(config->in_file, field->string_, MAX_FIELD_LEN);
    }
    
    field = json_find_member(node, "user");
    if (field != NULL) {
        strncpy(config->user, field->string_, MAX_FIELD_LEN);
    }

    field = json_find_member(node, "delay_start");
    if (field != NULL) {
        config->delay_start = field->number_;     
    }

    field = json_find_member(node, "delay_track");
    if (field != NULL) {
        config->delay_track = field->number_;     
    }

    field = json_find_member(node, "track_children");
    if (field != NULL) {
        config->track_children = field->number_;     
    }

    field = json_find_member(node, "once");
    if (field != NULL) {
        config->once = field->number_;     
    }
        
    return 0;
}


int get_jobs_json(job_t **jobs, size_t *count, char const *filename) {

    int  retcode;
    int  i;
    char buf[BUFFER_SIZE] = {};
    JsonNode *root_elem;
    JsonNode *jobs_elem;
    JsonNode *iter_elem;


    if ( (jobs == NULL) || (count == NULL) ) {
        return -1;
    }

    retcode = read_text_file(filename, buf, BUFFER_SIZE);
    if (retcode == -1) {
        return -1;
    }

    root_elem = json_decode(buf);
    if (root_elem == NULL) {
        return -1;
    }
    
    jobs_elem = json_find_member(root_elem, "jobs");
    if (jobs_elem == NULL) {
        return -1;
    }

    i = 0;
    json_foreach(iter_elem, jobs_elem) {
        if (i >= MAX_JOBS_NUM) {
            break;
        }

        retcode = complete_config_from_json(&s_jobs[i].config, iter_elem);
        if (retcode == -1) {
            printf("test\n");
            continue; // skip wrong node
        }

        i += 1;
    }
    
    *jobs  = &s_jobs[0];
    *count = i;

    json_delete(root_elem);
    
    return 0;
}
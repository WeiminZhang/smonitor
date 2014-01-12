#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <json.h>

#include "smonitor.h"
#include "config.h"

int read_text_file(char const *filename, char* buffer, size_t length)
{
    int retcode = 0;
    size_t elements_read = 0;
    FILE* fh = NULL;

    memset(buffer, 0, length);
    
    fh = fopen(filename, "r");
    if ( fh == NULL ) {
        return -1;
    }

    elements_read = fread(buffer, 1, length-1, fh);
    if ( elements_read == 0 ) {
        retcode = -1;
    }

    buffer[elements_read] = 0; // FIO17-C

    if ( fh != NULL) {
        fclose(fh);
    }

    return retcode;
}


int complete_job_config_from_json(config_t *config, JsonNode *node) {
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
    if (field != NULL && field->tag == JSON_STRING) {
        strncpy(config->workdir, field->string_, MAX_FIELD_LEN);
    }

    field = json_find_member(node, "out_file");
    if (field != NULL && field->tag == JSON_STRING) {
        strncpy(config->out_file, field->string_, MAX_FIELD_LEN);
    }

    field = json_find_member(node, "in_file");
    if (field != NULL && field->tag == JSON_STRING) {
        strncpy(config->in_file, field->string_, MAX_FIELD_LEN);
    }
    
    field = json_find_member(node, "user");
    if (field != NULL && field->tag == JSON_STRING) {
        strncpy(config->user, field->string_, MAX_FIELD_LEN);
    }

    field = json_find_member(node, "priority");
    if (field != NULL && field->tag == JSON_NUMBER) {
        config->priority = (uint32_t) field->number_;     
    }

    field = json_find_member(node, "delay_start");
    if (field != NULL && field->tag == JSON_NUMBER) {
        config->delay_start = (uint8_t) field->number_;     
    }

    field = json_find_member(node, "delay_track");
    if (field != NULL && field->tag == JSON_NUMBER) {
        config->delay_track = (uint8_t) field->number_;     
    }

    field = json_find_member(node, "once");
    if (field != NULL && field->tag == JSON_BOOL) {
        config->once = field->bool_;
    }

    field = json_find_member(node, "track_children");
    if (field != NULL && field->tag == JSON_BOOL) {
        config->track_children = field->bool_;
    }


    return 0;
}


int parse_config(char const *filename, smonitor_t* monitor) {
    int  retcode;
    int  i;
    char buf[BUFFER_SIZE] = {};
    JsonNode *root_elem;
    JsonNode *jobs_elem;
    JsonNode *iter_elem;


    if ( monitor == NULL ) {
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

    iter_elem = json_find_member(root_elem, "interval");
    if (iter_elem == NULL || iter_elem->tag != JSON_NUMBER) {
        monitor->interval = (uint64_t) DEFAULT_INTERVAL;
    } else {
        monitor->interval = (uint64_t) iter_elem->number_;
    }

    i = 0;
    json_foreach(iter_elem, jobs_elem) {
        if (i >= MAX_JOBS_NUM) {
            break;
        }

        retcode = complete_job_config_from_json(&(monitor->jobs[i].config), iter_elem);
        if (retcode == -1) {
            fprintf(stderr, "Wrong job description on %d position, skipping\n", i);
            continue;
        }

        i += 1;
    }
    monitor->jobs_num = i;

    json_delete(root_elem);

    return 0;
}

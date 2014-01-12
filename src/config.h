/**
 *******************************************************************************
 * Parse config file into internal data structure
 * @param[in] filename - path to json config file
 * @param[out] jobs - a pointer to smonitor_t variable
 * @return -1 if errror occured, otherwise 0
 ******************************************************************************/
int parse_config(char const *filename, smonitor_t* monitor);

int read_text_file(char const *filename, char* buffer, size_t length);
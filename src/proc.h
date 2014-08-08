#include <stddef.h>
#include <stdint.h>

int read_proc_file(char *buf, size_t buf_size, int pid, const char *name);

uint64_t proc_get_rss(pid_t pid);
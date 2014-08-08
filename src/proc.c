#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "proc.h"

int read_proc_file(char *buf, size_t buf_size, int pid, const char *name) {
	int fd;
	int bytes;
	int retcode = -1;
	char filename[256];

	snprintf(filename, sizeof(filename), "/proc/%d/%s", pid, name);

	if ((fd = open(filename, O_RDONLY)) < 0) {
		return retcode;
	}

	bytes = read(fd, buf, buf_size-1);
	if (bytes < 0) {
		goto error;
	}

	buf[bytes]='\0';
	retcode = 0;

	error:
		close(fd);

	return retcode;
}

uint64_t proc_get_rss(pid_t pid) {
	uint64_t retvalue= -1;
	int retcode = -1;
	char buf[256];

	memset(buf, 0, sizeof(buf));

	retcode = read_proc_file(buf, sizeof(buf), (int)pid, "statm");
	if (retcode == -1) {
		return -1;
	}

	retcode = sscanf(buf, "%*s%ld", &retvalue);
	if (retcode != 1) {
		return -1;
	}

	return retvalue;
}
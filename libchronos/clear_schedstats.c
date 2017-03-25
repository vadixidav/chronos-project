#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* Link against local copies, NOT outdated or non-existent ones */
#include "chronos.h"
#include "chronos_utils.h"

int main(int argc, char* argv[])
{
	unsigned long mask = -1;
	int r = 0, fd = open("/proc/sys/chronos/clear_on_sched_set", O_RDWR);
	char on = '1', buff;

	if(fd == -1) {
		perror("Cannot open proc file: are you sudo?\n"); exit(1);
	}

	r = read(fd, &buff, 1);
	write(fd, &on, 1);

	set_scheduler(SCHED_RT_FIFO, -1, mask);
	write(fd, &buff, r);

	close(fd);
	return 0;
}


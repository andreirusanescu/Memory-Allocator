// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int open(const char *filename, int flags, ...)
{
	int ret = syscall(__NR_open, filename, flags);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return ret;
}

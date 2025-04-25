#include <stdio.h>
#include <unistd.h>
#include <internal/syscall.h>
#include <errno.h>

int puts(const char *str)
{
	size_t i;
	for (i = 0; *(str + i) != '\0'; ++i) {}

	ssize_t ret = write(1, str, i);
	if (ret < 0) {
		return -1;
	}
	ret = write(1, "\n", 1);
	if (ret < 0) {
		return -1;
	}
	return i + 1;
}

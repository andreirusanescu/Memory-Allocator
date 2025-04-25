#include <unistd.h>
#include <time.h>

unsigned int sleep(unsigned int seconds)
{
	struct timespec req, rem;
	req.tv_nsec = 0;
	req.tv_sec = seconds;
	while (nanosleep(&req, &rem) == -1) {
		req = rem;
	}
	return rem.tv_sec;
}

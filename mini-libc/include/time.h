#ifndef _TIME_H_
#define _TIME_H_	1

#ifdef __cplusplus
extern "C" {
#endif

#include <internal/types.h>

struct timespec {
	long tv_sec;
	long tv_nsec;
};

int nanosleep(const struct timespec *req, struct timespec *rem);

#ifdef __cplusplus
}
#endif

#endif

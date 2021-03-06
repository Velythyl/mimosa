#ifndef _TIME_HEADER

#define _TIME_HEADER 1

#include "include/libc_header.h"

#ifdef USE_MIMOSA

#include "chrono.h"

#endif

struct timespec {
  int32 ts_sec;   // seconds
  int32 ts_nsec;  // nanoseconds
};

typedef int32 clock_t;
typedef int32 clockid_t;

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

extern clock_t REDIRECT_NAME(clock)(void);

extern int REDIRECT_NAME(nanosleep)(const struct timespec *__requested_time,
                                    struct timespec *__remaining);

extern int REDIRECT_NAME(clock_getres)(clockid_t __clock_id, struct timespec *__res);
extern int REDIRECT_NAME(clock_gettime)(clockid_t __clock_id, struct timespec *__tp);
extern int REDIRECT_NAME(clock_settime)(clockid_t __clock_id, const struct timespec *__tp);

#ifndef USE_LIBC_LINK

extern void libc_init_time(void);

#endif

#endif  // time.h

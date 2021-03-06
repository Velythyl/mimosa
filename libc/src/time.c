#include "include/libc_common.h"
#include "include/time.h"

clock_t REDIRECT_NAME(clock)(void) {

#ifdef USE_LIBC_LINK

  return LIBC_LINK._clock();

#else

  libc_trace("clock");

#ifdef USE_HOST_LIBC

  return clock();

#else

  // TODO: implement
  return 0;

#endif
#endif
}

int REDIRECT_NAME(nanosleep)(const struct timespec *__requested_time,
                             struct timespec *__remaining) {

#ifdef USE_LIBC_LINK

  return LIBC_LINK._nanosleep(__requested_time, __remaining);

#else

  libc_trace("nanosleep");

#ifdef USE_HOST_LIBC

  return nanosleep(__requested_time, __remaining);

#else

  // TODO: implement
  return 0;

#endif
#endif
}

int REDIRECT_NAME(clock_getres)(clockid_t __clock_id, struct timespec *__res) {

#ifdef USE_LIBC_LINK

  return LIBC_LINK._clock_getres(__clock_id, __res);

#else

  libc_trace("clock_getres");

#ifdef USE_HOST_LIBC

  return clock_getres(__clock_id, __res);

#else

  // TODO: implement
  return 0;

#endif
#endif
}

int REDIRECT_NAME(clock_gettime)(clockid_t __clock_id, struct timespec *__tp) {

#ifdef USE_LIBC_LINK

  return LIBC_LINK._clock_gettime(__clock_id, __tp);

#else

  libc_trace("clock_gettime");

#ifdef USE_HOST_LIBC

  return clock_gettime(__clock_id, __tp);

#else

  // TODO: implement
  return 0;

#endif
#endif
}

int REDIRECT_NAME(clock_settime)(clockid_t __clock_id, const struct timespec *__tp) {

#ifdef USE_LIBC_LINK

  return LIBC_LINK._clock_settime(__clock_id, __tp);

#else

  libc_trace("clock_settime");

#ifdef USE_HOST_LIBC

  return clock_settime(__clock_id, __tp);

#else

  // TODO: implement
  return 0;

#endif
#endif
}

#ifndef USE_LIBC_LINK

void libc_init_time(void) {
}

#endif

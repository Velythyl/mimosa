#ifndef _LIBC_LINK_HEADER

#define _LIBC_LINK_HEADER 1

#include "include/libc_header.h"
#include "include/dirent.h"
#include "include/errno.h"
#include "include/math.h"
#include "include/setjmp.h"
#include "include/stdio.h"
#include "include/stdlib.h"
#include "include/string.h"
#include "include/time.h"
#include "include/unistd.h"

struct libc_link {

  /* dirent.h */
  DIR *(*_opendir)(const char *__name);
  struct dirent *(*_readdir)(DIR *__dirp);
  int (*_closedir)(DIR *__dirp);

  /* errno.h */
  int *_errno;

  /* math.h */
  double (*_acos)(double __x);
  double (*_acosh)(double __x);
  double (*_asin)(double __x);
  double (*_asinh)(double __x);
  double (*_atan)(double __x);
  double (*_atan2)(double __y, double __x);
  double (*_atanh)(double __x);
  double (*_ceil)(double __x);
  double (*_cos)(double __x);
  double (*_cosh)(double __x);
  double (*_exp)(double __x);
  double (*_expm1)(double __x);
  double (*_fabs)(double __x);
  double (*_floor)(double __x);
  double (*_hypot)(double __x, double __y);
  int (*_ilogb)(double __x);
  double (*_log)(double __x);
  double (*_log1p)(double __x);
  double (*_modf)(double __x, double *__iptr);
  double (*_pow)(double __x, double __y);
  double (*_sin)(double __x);
  double (*_sinh)(double __x);
  double (*_sqrt)(double __x);
  double (*_tan)(double __x);
  double (*_tanh)(double __x);
  double (*_scalbn)(double __x, int __exp);

  /* setjmp.h */
  int (*_setjmp)(jmp_buf __env);
  void (*_longjmp)(jmp_buf __env, int __val);

  /* stdio.h */
  FILE *(*_fopen)(const char *__restrict __filename,
                  const char *__restrict __modes);
  FILE *(*_fdopen)(int __fd, const char *__modes);
  size_t (*_fread)(void *__restrict __ptr, size_t __size,
                   size_t __n, FILE *__restrict __stream);
  size_t (*_fwrite)(const void *__restrict __ptr, size_t __size,
                    size_t __n, FILE *__restrict __stream);
  int (*_fclose)(FILE *__stream);
  int (*_fflush)(FILE *__stream);
  int (*_fseek)(FILE *__stream, long __off, int __whence);
  long (*_ftell)(FILE *__stream);
  void (*_clearerr)(FILE *__stream);
  int (*_ferror)(FILE *__stream);
  int (*_rename)(const char *__oldpath, const char *__newpath);
  int (*_fprintf_aux)(FILE *__stream, const char **__format);
  FILE *_stdin;
  FILE *_stdout;
  FILE *_stderr;

  /* stdlib.h */
  void *(*_malloc)(size_t __size);
  void (*_free)(void *__ptr);
  void (*_exit)(int __status);
  char *(*_getenv)(const char *__name);
  int (*_system)(const char *__command);

  /* string.h */
#if 0
  void *(*_memcpy)(void *__restrict __dest, const void *__restrict __src,
                   size_t __n);
  void *(*_memmove)(void *__dest, const void *__src, size_t __n);
#endif

  /* time.h */
  clock_t (*_clock)(void);
  time_t (*_time)(time_t *__timer);

  /* unistd.h */
  char *(*_getcwd)(char *__buf, size_t __size);
  int (*_mkdir)(const char *__pathname, mode_t __mode);
  int (*_remove)(const char *__pathname);
  int (*_stat)(const char *__pathname, struct_stat *__buf);
  int (*_lstat)(const char *__pathname, struct_stat *__buf);

};

#ifdef USE_MIMOSA
#define LIBC_LINK (*(struct libc_link *)0x1F000)
#else
extern struct libc_link LIBC_LINK;
#endif

extern void libc_init(void);

#endif /* libc_link.h */

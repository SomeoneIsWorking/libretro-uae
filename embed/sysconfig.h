#ifndef UAE_M68K_EMBED_SYSCONFIG_H
#define UAE_M68K_EMBED_SYSCONFIG_H

#include "../retrodep/sysconfig.h"

/* retrodep describes its POSIX libretro hosts. The embed target also supports
 * the native Windows CRT, where these headers are not available. */
#if defined(UAE_M68K_EMBED) && defined(_WIN32)
#undef HAVE_DIRENT_H
#undef HAVE_STRINGS_H
#undef HAVE_SYS_IOCTL_H
#undef HAVE_SYS_PARAM_H
#undef HAVE_SYS_TERMIOS_H
#undef HAVE_SYS_TIME_H
#undef HAVE_UNISTD_H
#undef HAVE_UTIME_H
#undef TIME_WITH_SYS_TIME
#endif

#endif

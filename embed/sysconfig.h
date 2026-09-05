#ifndef UAE_M68K_EMBED_SYSCONFIG_H
#define UAE_M68K_EMBED_SYSCONFIG_H

#include "../retrodep/sysconfig.h"

/* This target owns only the 68000 prefetch core. Do not instantiate tables for
 * CPU models whose handlers and MMU/device owners are outside this library. */
#define CPUEMU_68000_ONLY
#undef CPUEMU_0
#undef CPUEMU_13
#undef CPUEMU_20
#undef CPUEMU_21
#undef CPUEMU_22
#undef CPUEMU_23
#undef CPUEMU_24
#undef CPUEMU_25
#undef CPUEMU_31
#undef CPUEMU_32
#undef CPUEMU_33
#undef CPUEMU_34
#undef CPUEMU_35
#undef CPUEMU_40
#undef CPUEMU_50

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

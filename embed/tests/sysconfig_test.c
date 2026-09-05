#include "../sysconfig.h"

#if !defined(CPUEMU_68000_ONLY) || !defined(CPUEMU_11)
#error "The embedded CPU requires the 68000 prefetch instruction profile"
#endif
#if defined(CPUEMU_0) || defined(CPUEMU_13) || defined(CPUEMU_20) || defined(CPUEMU_21) || \
    defined(CPUEMU_22) || defined(CPUEMU_23) || defined(CPUEMU_24) || defined(CPUEMU_25) || \
    defined(CPUEMU_31) || defined(CPUEMU_32) || defined(CPUEMU_33) || defined(CPUEMU_34) || \
    defined(CPUEMU_35) || defined(CPUEMU_40) || defined(CPUEMU_50)
#error "The embedded CPU may not instantiate unrelated CPU tables"
#endif

/* This compiles both with the real host feature set and with the Windows CRT
 * feature set. It needs no platform headers, so POSIX hosts exercise both arms. */
#if defined(_WIN32)
#if defined(HAVE_DIRENT_H) || defined(HAVE_STRINGS_H) || defined(HAVE_SYS_IOCTL_H) || \
    defined(HAVE_SYS_PARAM_H) || defined(HAVE_SYS_TERMIOS_H) || defined(HAVE_SYS_TIME_H) || \
    defined(HAVE_UNISTD_H) || defined(HAVE_UTIME_H) || defined(TIME_WITH_SYS_TIME)
#error "The embedded Windows CPU feature set advertises unavailable POSIX headers"
#endif
#else
#if !defined(HAVE_DIRENT_H) || !defined(HAVE_STRINGS_H) || !defined(HAVE_SYS_IOCTL_H) || \
    !defined(HAVE_SYS_PARAM_H) || !defined(HAVE_SYS_TERMIOS_H) || !defined(HAVE_SYS_TIME_H) || \
    !defined(HAVE_UNISTD_H) || !defined(HAVE_UTIME_H) || !defined(TIME_WITH_SYS_TIME)
#error "The embedded POSIX CPU feature set lost its native header definitions"
#endif
#endif

int main(void) {
    return 0;
}

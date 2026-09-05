#include "../sysconfig.h"

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

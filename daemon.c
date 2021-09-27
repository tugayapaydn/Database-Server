#include "include/daemon.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>


int bcdaemon(int flags) {
    switch (fork()) {
        case -1: return -1;
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        return -1;

    switch (fork()) {
        case -1: return -1;
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if (!(flags & NO_UMASK0))
        umask(0);

    if (!(flags & NO_CHDIR))
        chdir("/");

    if (!(flags & NO_CLOSE_FILES)) {
        int fd = sysconf(_SC_OPEN_MAX);
        if (fd == -1)
            fd = MAX_CLOSE;

        for (int i = 0; i < fd; i++) {
            close(i);
        }
    }

    if (!(flags & NO_REOPEN_STD_FDS)) {
        close(STDIN_FILENO);

        int fd = open("/dev/null", O_RDWR);

        if (fd != STDIN_FILENO)
            return -1;

        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -1;
        
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -1;
    }

    return 0;
}
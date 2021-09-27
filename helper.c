#include "include/helper.h"

int set_sig_handler(void (*f)(int)) {
    struct sigaction usr;
	usr.sa_handler = f;
    usr.sa_flags = 0;
    
	if (sigemptyset(&usr.sa_mask) == -1 || sigaction(SIGINT, &usr, NULL) == -1 || sigaction(SIGPIPE, &usr, NULL) == -1)
        return -1;

    return 0;
}

int sig_crit_block() {
    sigset_t new_mask;

    if(sigemptyset(&new_mask) == -1 || sigaddset(&new_mask, SIGTERM) == -1
        || sigaddset(&new_mask, SIGINT) == -1 || sigaddset(&new_mask, SIGSTOP) == -1 
            || sigaddset(&new_mask, SIGCONT) == -1 || sigprocmask(SIG_BLOCK, &new_mask, NULL) == -1)  
		return -1;
	
    return 0;
}

int sig_crit_unblock() {
    sigset_t new_mask;

    if(sigemptyset(&new_mask) == -1 || sigaddset(&new_mask, SIGTERM) == -1
        || sigaddset(&new_mask, SIGINT) == -1 || sigaddset(&new_mask, SIGSTOP) == -1 
            || sigaddset(&new_mask, SIGCONT) == -1 || sigprocmask(SIG_UNBLOCK, &new_mask, NULL) == -1)  
		return -1;
	
    return 0;
}

int file_lock(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLKW, &lock) == -1)
        return -1;
    
    return 0;
}

int file_unlock(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if(fcntl(fd, F_SETLKW, &lock) == -1)
        return -1;

    return 0;
}

void free_2d(int** arr, int size) {
    if (arr != NULL) {
        for (size_t i = 0; i < size; i++) {
            if (arr[i] != NULL) {
                free(arr[i]);
                arr[i] = NULL;
            }
        }
        free(arr);
        arr = NULL;
    }
}

void close_2d(int** arr, int r, int c) {
    if (arr != NULL) {
        for (size_t i = 0; i < r; i++) {
            if (arr[i] != NULL) {
                for (size_t j = 0; j < c; j++) {
                    close(arr[i][j]);
                }
            }
        }
    }
}
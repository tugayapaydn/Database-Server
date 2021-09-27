#ifndef HELPER_H_
#define HELPER_H_

#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define READ_PERMS 	(S_IRUSR | S_IRGRP | S_IROTH)
#define WRITE_PERMS (S_IWUSR | S_IWGRP | S_IWOTH)

int set_sig_handler(void (*f)(int));
int sig_crit_block();
int sig_crit_unblock();
int file_lock(int fd);
int file_unlock(int fd);
void free_2d(int** arr, int size);
void close_2d(int** arr, int r, int c);

#endif /* HELPER_H_ */
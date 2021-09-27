#ifndef FILEOP_H_
#define FILEOP_H_

#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define READ_PERMS 	(S_IRUSR | S_IRGRP | S_IROTH)
#define WRITE_PERMS (S_IWUSR | S_IWGRP | S_IWOTH)

int* init_line_stats(int fd, int* size);
off_t get_line_offset(int fd, int line);
char* readline(int fd, int line, off_t* offset);
char* readnextline(int fd);
char* sreadnextline(int fd, int size);
int insert_to_file(int fd, char* buffer, int size, off_t pos);
int line_count(int fd);

#endif /* FILEOP_H_ */
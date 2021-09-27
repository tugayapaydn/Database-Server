#ifndef TABLE_H_
#define TABLE_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#define READ_PERMS 	(S_IRUSR | S_IRGRP | S_IROTH)
#define WRITE_PERMS (S_IWUSR | S_IWGRP | S_IWOTH)

typedef struct TABLE {
    int started;
    int fd;
    long ncolumn;
    long nrows;

    char*** table;

} TABLE;

TABLE table;

int active_reader;
int active_writer;
int waiting_reader;
int waiting_writer;

pthread_cond_t okToRead;
pthread_cond_t okToWrite;
pthread_mutex_t rw_lock;

int table_start(char* file);
int table_load();
void table_end();

char* query_parser(char* query, long *records);

#endif /* TABLE_H_ */
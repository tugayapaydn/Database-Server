#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>

#include "helper.h"
#include "table.h"
#include "daemon.h"

#define UNIQUE_NAME "UNIQUE_SERVER_PROG_XXXXXXXXX"

#define MAX_QUERY_SIZE 300
#define DOMAIN AF_INET
#define MAX_REQUEST 1000

typedef enum TH_STATUS {AVAILABLE, BUSY} TH_STATUS;

typedef struct TH_DATA {
    pthread_t pid;
    int id;
    int clientfd;
    TH_STATUS status;
    char* output;

    pthread_mutex_t wait;
} TH_DATA;

sem_t *unique_test = NULL;

int THREAD_EXIT = 0;
int lf;

int sockfd = -1;
int PORT;
int poolSize;
char* logFile = NULL;
char* datasetPath = NULL;

TH_DATA *thData_list = NULL;

//Thread synchronization
int max_threads = 0;
int available_threads = 0;
pthread_cond_t avail_th = PTHREAD_COND_INITIALIZER;
pthread_mutex_t avail_th_m = PTHREAD_MUTEX_INITIALIZER;

void read_commandline(int argc, char* argv[]);
void full_exit();
void set_logfile();
void load_dataset();
void* thread_func(void* args);
int connect_server(struct sockaddr_in *serv_addr);
int find_avail_thread();
int init_threads();
void set_unique();
int server(int argc, char* argv[]);

#endif /* SERVER_H_ */
#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "fileop.h"
#include "helper.h"

#define DOMAIN AF_INET
#define MAX_READ_BUFFER 4096

char* IPv4 = NULL;
char* query_file = NULL;
int PORT;
int id;
struct sockaddr_in client_addr;

void read_commandline(int argc, char* argv[]);
void full_exit();

void init_stream();
void process_query(char *query);

size_t read_msg_size(int sd);
size_t read_record_size(int sd);

#endif /* CLIENT_H_ */
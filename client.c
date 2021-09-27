#include "include/client.h"

// Signal handler to handle SIGINT
void sig_handler(int signum) {
    if (signum == SIGINT) {
        exit(EXIT_SUCCESS);
    }
}

char* buffer = NULL;
char* msg = NULL;
int fd = -1;
int sd = -1;

int main(int argc, char* argv[]) {
    set_sig_handler(&sig_handler);  // Set signal handler
    atexit(full_exit);              // Set exit function to be called at exit.
    read_commandline(argc, argv);   // Read commandline arguments

    fprintf(stdout, "%lu|\t Client-%d connecting to %s:%d\n" ,(unsigned long)time(NULL), id, IPv4, PORT);

    init_stream();

    // QUERY PARSER
    while ((buffer = readnextline(fd)))     // Read the next line from the query file
    {
        char *save, *temp = strtok_r(buffer, " ", (char**)&save);   // Parse the first token
        
        // Read client id
        errno = 0;
        int cid = strtol(temp, (char**)NULL, 10);
        if (errno != 0) {
            fprintf(stderr, "%lu|\t Client %d has failed to parse a query!\n" ,(unsigned long)time(NULL), id);
            exit(EXIT_FAILURE);
        }
        
        // If query belongs to this client
        if (cid == id) {
            fprintf(stdout, "%lu|\t Client-%d connected and sending query '%s'\n" ,(unsigned long)time(NULL), id, save);
            process_query(save);    // Call query processer function
        }
        
        free(buffer);
        buffer = NULL;
    }

    exit(EXIT_SUCCESS);
}

/*  Process query function that sends the query to the server and receives the server response.
*/
void process_query(char *query) {
    if (query == NULL) {
        fprintf(stderr, "%lu|\t Client %d has failed to parse a query! (2)\n",(unsigned long)time(NULL), id);
        exit(EXIT_FAILURE);
    }

    // Send the query to the server
    if (write(sd, query, strlen(query)) < 0) {
        fprintf(stdout, "%lu|\t Client-%d has failed to send the query to the server.\n" ,(unsigned long)time(NULL), id);
        exit(EXIT_FAILURE);
    }

    clock_t t = clock();
    size_t size = read_msg_size(sd), records = read_record_size(sd);

    if ((msg = malloc(sizeof(char)*(size+1))) == NULL) {
        fprintf(stdout, "%lu|\t Client-%d has failed to read the response from the server.\n" ,(unsigned long)time(NULL), id);
        exit(EXIT_FAILURE);
    } 
    memset(msg, 0, size+1);
    
    // Reading the server message block by block by MAX_READ_BUFFER size.
    char buffer[MAX_READ_BUFFER] = {0};
    int ret;
    size_t total_size = 0;
    do {
        // Read message block
        if ((ret = read(sd, buffer, MAX_READ_BUFFER-1)) < 0) {
            fprintf(stdout, "%lu|\t Client-%d has failed to read the response from the server.\n" ,(unsigned long)time(NULL), id);
            exit(EXIT_FAILURE);
        }
        if (total_size == 0) {
            strcpy(msg, buffer);
        } else
            strcat(msg, buffer);
        total_size += ret;

        memset(buffer, 0, MAX_READ_BUFFER);
    
    } while (total_size < size);

    t = clock() - t;
    fprintf(stdout, "%lu|\t Server’s response to Client-%d is %lu records, and arrived in %f seconds.\n"
                , (unsigned long)time(NULL), id, records, (((double) t) / CLOCKS_PER_SEC));

    // Print the message
    fprintf(stdout, "%s\n", msg);

    free(msg);
    msg = NULL;
}

// The first block of the server response is the message size.
size_t read_msg_size(int sd) {
    char size[20] = {0};
    
    // Read message size
    if (read(sd, size, 20) < 0) {
        fprintf(stdout, "%lu|\t ERROR: Failed to read message size!\n" ,(unsigned long)time(NULL));    
        exit(EXIT_FAILURE);
    }
    
    errno = 0;
    int ret = strtol(size, (char**)NULL, 10);
    if (errno != 0) {
        fprintf(stdout, "%lu|\t ERROR: Failed to read message size!\n" ,(unsigned long)time(NULL));    
        exit(EXIT_FAILURE);
    }

    return ret;
}

// The second block of the server response is the records size.
size_t read_record_size(int sd) {
    char size[20] = {0};
    
    // Read record size
    if (read(sd, size, 20) < 0) {
        fprintf(stdout, "%lu|\t ERROR: Failed to read record size!\n" ,(unsigned long)time(NULL));    
        exit(EXIT_FAILURE);
    }
    
    errno = 0;
    int ret = strtol(size, (char**)NULL, 10);
    if (errno != 0) {
        fprintf(stdout, "%lu|\t ERROR: Failed to read record size!\n" ,(unsigned long)time(NULL));    
        exit(EXIT_FAILURE);
    }

    return ret;
}

// Initialize socket stream and connect to the server
void init_stream() {
    if ((fd = open(query_file, O_RDONLY, READ_PERMS)) < 0) {
        fprintf(stderr, "%lu|\t ERROR: Client %d has failed to open query file. Make sure that the path is correct.\n",(unsigned long)time(NULL), id);
        exit(EXIT_FAILURE);
    }
    
    // Init socket
    if ((sd = socket(DOMAIN, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "%lu|\t ERROR: Client %d has failed to create the socket stream.\n",(unsigned long)time(NULL), id);
        exit(EXIT_FAILURE);
    }
    
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_family = DOMAIN;
    client_addr.sin_port = htons(PORT);
    client_addr.sin_addr.s_addr = inet_addr(IPv4);
    
    // Connect to the server
    if (connect(sd, (struct sockaddr*)(&client_addr), sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "%lu|\t ERROR: Client %d has failed to connect to the server.\n",(unsigned long)time(NULL), id);
        exit(EXIT_FAILURE);
    }
}

void full_exit() {
    fprintf(stdout, "%lu|\t Client-%d is terminating...\n" ,(unsigned long)time(NULL), id);

    if (query_file) free(query_file);
    if (IPv4)   free(IPv4);
    if (buffer) free(buffer);
    if (msg)    free(msg);

    if (fd > 0) close(fd);
    fd = -1;
    if (sd > 0) close(sd);
    sd = -1;
}

void read_commandline(int argc, char* argv[]) {
    int opt;
    errno = 0;

    /* Read commandline arguments */
    while ((opt = getopt(argc, argv, ":i:a:p:o:")) != -1) {
        switch (opt) {
        case 'i':
            id = (int)strtol(optarg, (char**)NULL, 10);
            break;

        case 'a':
            IPv4 = strdup(optarg);
            break;

        case 'p':
            PORT = (int)strtol(optarg, (char**)NULL, 10);
            break;

        case 'o':
            query_file = strdup(optarg);
            break;

        default:
            break;
        }
    }

    if (errno != 0 || IPv4 == NULL || query_file == NULL || PORT <= 1000 || id < 1) {
        fprintf(stderr, "%lu|\t ERROR: Failed to start a client, commandline is wrong\n"
                    "Commandline Usage: -[apoi]\n"
                    "-a: IPv4 address of the machine running the server.\n"
                    "-p: Port number at which the server waits for connections(>1000).\n"
                    "-o: Relative or absolute path of the file containing an arbitrary number of queries.\n"
                    "-i: Integer id of the client (>=1).\n"
                    ,(unsigned long)time(NULL));

        exit(EXIT_FAILURE);
    }
}


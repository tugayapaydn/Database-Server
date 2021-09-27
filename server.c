#include "include/server.h"

// Signal handler
void signal_handler(int signum) {
    switch (signum) {
    case SIGINT:
        exit(EXIT_SUCCESS);
        break;
    
    case SIGPIPE:
        break;
    default:
        break;
    }
}

int main(int argc, char* argv[]) {
    set_unique();                                                               // Set a lock that avoids double instantiation of the server.
    atexit(full_exit);                                                          // Set exit function.
    read_commandline(argc, argv);                                               // Read commandline arguments
    bcdaemon((NO_CHDIR | NO_CLOSE_FILES | NO_REOPEN_STD_FDS | NO_UMASK0));      // Set daemon process
    set_logfile();                                                              // Open the logfile
    set_sig_handler(&signal_handler);                                           // Set signal handler
    
    server(argc, argv);                                                         // Start the server

    exit(EXIT_SUCCESS);
}


// Main server function
int server(int argc, char* argv[]) {
    int clientfd;
    struct sockaddr_in serv_addr;

    // Connect to the socket
    if (connect_server(&serv_addr) == -1) {
        dprintf(lf, "%lu|\t Server has failed to accept upcoming client connections.\n" ,(unsigned long)time(NULL));
        exit(EXIT_FAILURE);
    }
    
    // Load csv file to the table
    sig_crit_block();
    load_dataset();
    sig_crit_unblock();
    
    // Initialize threads
    sig_crit_block();
    init_threads();
    sig_crit_unblock();

    // Main server while loop
    while (1) {
        socklen_t size = sizeof(struct sockaddr_in);
        
        // Accept a connection from a client
		if ((clientfd = accept(sockfd, ((struct sockaddr*)&serv_addr), &size)) == -1) {
			dprintf(lf, "%lu|\t Server has failed to accept upcoming client connections.\n" ,(unsigned long)time(NULL));
			exit(EXIT_FAILURE);
		}

        // Wait for at least 1 available thread in the pool
        pthread_mutex_lock(&avail_th_m);
        while(available_threads == 0) {
            if (max_threads <= 0) {
                dprintf(lf, "%lu|\t No thread is available and all threads were exited abnormally. Server is shutting down.\n" ,(unsigned long)time(NULL));
                pthread_mutex_unlock(&avail_th_m);
                exit(EXIT_FAILURE);
            }
            else {
                dprintf(lf, "%lu|\t No thread is available! Waiting\n" ,(unsigned long)time(NULL));
            }

            pthread_cond_wait(&avail_th, &avail_th_m);
        }
        pthread_mutex_unlock(&avail_th_m);

        // Find an available thread
        int th_in = find_avail_thread();
        if (th_in == -1) {
            dprintf(lf, "%ld|\t ERROR: Failed to find an available thread!\n" ,(unsigned long)time(NULL));
            exit(EXIT_FAILURE);
        }

        dprintf(lf, "%ld|\t A connection has been delegated to thread id #%d\n" ,(unsigned long)time(NULL), th_in);

        // Set client to the thread
        thData_list[th_in].clientfd = clientfd;
        thData_list[th_in].status = BUSY;
        --available_threads;
        pthread_mutex_unlock(&thData_list[th_in].wait);
    }

    return 0;
}

// Connect to the socket (bind and listen)
int connect_server(struct sockaddr_in *serv_addr) {
    serv_addr->sin_family = DOMAIN;
	serv_addr->sin_port = htons(PORT);
	serv_addr->sin_addr.s_addr = htonl(INADDR_ANY);

    if ((sockfd = socket(DOMAIN, SOCK_STREAM, 0)) == -1 ||
        (bind(sockfd, (struct sockaddr*)serv_addr, sizeof(struct sockaddr_in)) == -1) ||
        (listen(sockfd, MAX_REQUEST) == -1) ) {	
		return -1;
	}

    return 0;
}

// Thread abnormal exit
void* thread_abnormal_return() {
    pthread_mutex_lock(&avail_th_m);
    --max_threads;
    pthread_cond_signal(&avail_th);
    pthread_mutex_unlock(&avail_th_m);
    return ((void*) -1);
}

// Main thread function
void* thread_func(void* args) {
    TH_DATA* mydata = (TH_DATA*)args;
    char* output = mydata->output;
    
    char query[MAX_QUERY_SIZE];

    // Start executing main thread loop
    while (1) {
        if (THREAD_EXIT) {  // If exit signal arrived, return.
            dprintf(lf, "%lu|\t Thread #%d received exit signal.\n" ,(unsigned long)time(NULL),mydata->id);
            pthread_mutex_unlock(&mydata->wait);
            return ((void*) 0);
        }

        // Set thread to 'AVAILABLE' status and wait for a connection.
        pthread_mutex_lock(&avail_th_m);
        mydata->status = AVAILABLE;
        ++available_threads;
        pthread_cond_signal(&avail_th);
        pthread_mutex_unlock(&avail_th_m);

        dprintf(lf, "%lu|\t Thread #%d: Waiting for connection.\n" ,(unsigned long)time(NULL), mydata->id);

        //Wait main thread to take next query
        pthread_mutex_lock(&mydata->wait);

        if (THREAD_EXIT) {  // If exit signal arrived, return.
            dprintf(lf, "%lu|\t Thread #%d received exit signal.\n" ,(unsigned long)time(NULL),mydata->id);
            pthread_mutex_unlock(&mydata->wait);
            return ((void*) 0);
        }

        // Client process loop.
        int ret;
        do {
            // Read a query from the client
            memset(query, 0, MAX_QUERY_SIZE);
            if ((ret = read(mydata->clientfd, query, MAX_QUERY_SIZE-1)) < 0) 
            {
                dprintf(lf, "%lu|\t Thread #%d has failed to read the client request.\n" ,(unsigned long)time(NULL),mydata->id);
                return thread_abnormal_return();
            } 
            else if (ret != 0) 
            {
                dprintf(lf, "%lu|\t Thread #%d: received query '%s'\n" ,(unsigned long)time(NULL),mydata->id, query);

                long records = 0;
                char stroutputsize[20] = {0};
                char str_records[20] = {0};

                // Call query parser function
                if ((output = query_parser(query, &records)) == NULL) {
                    dprintf(lf, "%lu|\t Thread #%d has failed to parse client request\n" ,(unsigned long)time(NULL),mydata->id);
                    return thread_abnormal_return();
                }
                
                // Write message size, number of records that will be returned and message to the client socket.
                if ((snprintf(stroutputsize, 20, "%lu" ,strlen(output)) < 0) ||
                    (snprintf(str_records, 20, "%ld" ,records) < 0) ||
                    (write(mydata->clientfd, stroutputsize, 20) < 0) ||
                    (write(mydata->clientfd, str_records, 20) < 0) ||
                    (write(mydata->clientfd, output, strlen(output)) < 0)) 
                {
                    dprintf(lf, "%lu|\t Thread #%d has failed to write client request\n" ,(unsigned long)time(NULL),mydata->id);
                    if (strcmp(output, "SUCCESS\n") != 0) free(output);
                    output = NULL;
                    return thread_abnormal_return();
                }
                
                dprintf(lf, "%lu|\t Thread #%d: query completed, %ld records have been returned.\n" ,(unsigned long)time(NULL),mydata->id, records);
                
                if (strcmp(output, "SUCCESS\n") != 0) free(output);
                output = NULL;

                usleep(500000);
            }
            else {
                dprintf(lf, "%lu|\t Thread #%d: No query has been delivered. Continuing...\n" ,(unsigned long)time(NULL),mydata->id);
            }
        } while (ret > 0 && !THREAD_EXIT);
    }

    return ((void*) 0);
}

// Creates an exclusive named semaphore to make sure that the server can only have one instant.
void set_unique() {
    if ((unique_test = sem_open(UNIQUE_NAME, (O_CREAT | O_EXCL), O_RDWR, 0)) == SEM_FAILED) {
        fprintf(stderr, "%lu|\t There is another instant of the server already running. Exiting...\n" ,(unsigned long)time(NULL));
        exit(EXIT_FAILURE);
    }
    sem_close(unique_test);
}

// Initializes thread datas and creates threads.
int init_threads() {
    if ((thData_list = malloc(sizeof(TH_DATA)*poolSize)) == NULL ) {
        return -1;
    }

    for (int i = 0; i < poolSize; i++) {
        thData_list[i].id = i;
        thData_list[i].status = BUSY;
        thData_list[i].output = NULL;
        if (pthread_mutex_init(&thData_list[i].wait, NULL) != 0 ||
            pthread_mutex_lock(&thData_list[i].wait)
        ) {
            dprintf(lf, "%lu|\t ERROR: Failed to create thread %d!\n" ,(unsigned long)time(NULL), (int)i);
        }
    }

    for (size_t i = 0; i < poolSize; i++) {
        if (pthread_create(&thData_list[i].pid , NULL, &thread_func, &thData_list[i]) != 0) {
            dprintf(lf, "%lu|\t ERROR: Failed to create thread %d!\n" ,(unsigned long)time(NULL), (int)i);
        }
    }

    return 0;
}

// Returns the index of an available thread from the thread pool (array).
int find_avail_thread() {
    pthread_mutex_lock(&avail_th_m);
    for (size_t i = 0; i < poolSize; i++) {
        if (thData_list[i].status == AVAILABLE) {
            pthread_mutex_unlock(&avail_th_m);
            return i;
        }
    }
    pthread_mutex_unlock(&avail_th_m);
    return -1;
}

// Start the database table and load csv file into it.
void load_dataset() {
    clock_t t = clock();

    dprintf(lf, "%lu|\t Loading dataset...\n", (unsigned long)time(NULL));

    if (table_start(datasetPath) < 0 || 
        table_load() < 0) {
        dprintf(lf, "%lu|\t ERROR: Failed to load the dataset!\n", (unsigned long)time(NULL));
        exit(EXIT_FAILURE);
    }

    t = clock() - t;
    dprintf(lf, "%lu|\t Dataset loaded in %f seconds with %ld records.\n", (unsigned long)time(NULL), (((double) t) / CLOCKS_PER_SEC), table.nrows);
}

// Open the log file.
void set_logfile() {
    if ((lf = open(logFile, O_RDWR | O_CREAT | O_TRUNC, (READ_PERMS | WRITE_PERMS))) < 0)  {
        fprintf(stderr, "%lu|\t ERROR: Server has failed to create the logfile.\n", (unsigned long)time(NULL));
        exit(EXIT_FAILURE);
    }
}

void full_exit() {    
    dprintf(lf, "%lu|\t Termination signal received, waiting for ongoing threads to complete.\n" ,(unsigned long)time(NULL));
    THREAD_EXIT = 1;
    if (thData_list != NULL) {
        for (size_t i = 0; i < poolSize; i++)    {
            pthread_mutex_unlock(&thData_list[i].wait);
            if (pthread_join(thData_list[i].pid, NULL) != 0) {
                dprintf(lf, "%lu|\t ERROR: Failed to join thread %d!\n" ,(unsigned long)time(NULL), (int)i);
            }
            if (thData_list[i].output) free(thData_list[i].output);
        }
    }

    dprintf(lf, "%lu|\t All threads have terminated, server shutting down.\n" ,(unsigned long)time(NULL));

    if (datasetPath) free(datasetPath);
    if (logFile) free(logFile);
    if (thData_list) free(thData_list);
    table_end();
    if (sockfd > 0) close(sockfd);
    if (unique_test != NULL) sem_unlink(UNIQUE_NAME);
}

void read_commandline(int argc, char* argv[]) {
    int opt;
    errno = 0;

    /* Read commandline arguments */
    while ((opt = getopt(argc, argv, ":p:o:l:d:")) != -1) {
        switch (opt) {
        case 'p':
            PORT = (int)strtol(optarg, (char**)NULL, 10);
            break;

        case 'o':
            logFile = strdup(optarg);
            break;

        case 'l':
            poolSize = (int)strtol(optarg, (char**)NULL, 10);
            max_threads = poolSize;
            break;

        case 'd':
            datasetPath = strdup(optarg);
            break;

        default:
            break;
        }
    }

    if (errno != 0 || logFile == NULL || datasetPath == NULL || poolSize < 2) {
        fprintf(stderr, "%ld|\t ERROR: Failed to start the server, commandline is wrong\n"
                    "Commandline Usage: -[pold]\n"
                    "-p: Port number at which the server waits for connections(>1000).\n"
                    "-o: Relative or absolute path of the log file.\n"
                    "-l: The number of threads in the pool.\n"
                    "-d: Relative or absolute path of a csv file containing a single table.\n"
                    ,(unsigned long)time(NULL));

        exit(EXIT_FAILURE);
    }
}

void table_print() {
        
    printf("ROWS: %ld, COLUMNS: %ld\n" ,table.nrows, table.ncolumn);

    if (table.table) {
        for (size_t i = 0; i < table.nrows; i++) {
            for (size_t j = 0; j < table.ncolumn; j++) {
                
                if (table.table[j][i]) {
                    
                    if (strlen(table.table[j][i]) != 0)
                        dprintf(lf, "%s,", table.table[j][i]);
                    else
                        dprintf(lf, "NONE,");
                }
            }
            dprintf(lf, "\n");
        }
    }
}
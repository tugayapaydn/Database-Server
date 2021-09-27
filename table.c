#include "include/table.h"

/* Reader start function must be called when a reading process occurs.
 * It defines that a reader is inside the critical section.
 */
void reader_start() {
    pthread_mutex_lock(&rw_lock);
    
    while ((active_writer + waiting_writer) > 0) {
        ++waiting_reader;
        pthread_cond_wait(&okToRead, &rw_lock);
        --waiting_reader;
    }
    ++active_reader;
    pthread_mutex_unlock(&rw_lock);
}

/* Reader end function must be called at the end of a reading process.
 * It defines that a reader is leaving the critical section.
 */
void reader_end() {
    pthread_mutex_lock(&rw_lock);
    --active_reader;
    if (active_reader == 0 && waiting_writer > 0) 
        pthread_cond_signal(&okToWrite);
    pthread_mutex_unlock(&rw_lock);
}

/* Writer start function must be called when a writing process occurs.
 * It defines that a writer is inside the critical section.
 */
void writer_start() {
    pthread_mutex_lock(&rw_lock);

    while ((active_reader + active_writer) > 0) {
        ++waiting_writer;
        pthread_cond_wait(&okToWrite, &rw_lock);
        --waiting_writer;
    }

    ++active_writer;
    pthread_mutex_unlock(&rw_lock);
}

/* Writer end function must be called at the end of a writing process.
 * It defines that a writer is leaving the critical section.
 */
void writer_end() {
     pthread_mutex_lock(&rw_lock);
     --active_writer;

    if (waiting_writer > 0)
        pthread_cond_signal(&okToWrite);
    else if (waiting_reader > 0)
        pthread_cond_broadcast(&okToRead);

     pthread_mutex_unlock(&rw_lock);
}

/* Calculates row and column counts of the table */
int rc_stats(int fd, long *r, long *c) {
    if (lseek(fd, 0, SEEK_SET) < 0) 
        return -1;
    
    *r = 1;
    *c = 1;

    char ch, prev;
    int ret;

    // Read lines
    while ((ret = read(fd, &ch, 1)) > 0) 
    {
        if (ch == '\"') {
            char del = ch;

            while ((ret = read(fd, &ch, 1) > 0) && ch != del);
            if (ret < 0 || ret == 0)
                return -1;
        }
        else if (ch == '\n' && prev != '\n') {
            ++(*r);
        }
        else if (ch == ',' && (*r) == 1) {
            // If it is first line, calculate column numbers
            ++(*c);
        }
    
        if (ch != '\r')
            prev = ch;
    }

    if (prev == '\n')
        --(*r);

    if (ret < 0)
        return -1;
 
    return 0;
}

/* Reads next line from the file, return dynamically allocated data. */
char* readnextline(int fd) {
    char c;
    char* buffer;
    int size = 0;
    
    off_t offset = lseek(fd, 0, SEEK_CUR);

    // Read size of the line
    int ret;
    while ((ret = read(fd, &c, 1)) > 0 && c != '\n') { 
        if (c == '\"') {
            char del = c;
            do {
                size++;
            } while ((ret = read(fd, &c, 1) > 0) && c != del);
            if (ret < 0 || ret == 0)
                return NULL;
        }

        if (c != '\r')
            size++;
    }

    if (size == 0) 
        return NULL;

    if (lseek(fd, offset, SEEK_SET) == -1)
        return NULL;
    
    if ((buffer = ((char*)malloc(size+1))) == NULL) 
        return NULL;

    memset(buffer, 0, size+1);
    
    // Read the line according to size
    size_t i = 0;
    while (i < size+1 && (ret = read(fd, &c, 1)) > 0 && c != '\n') { 
        if (c == '\"') {
            char del = c;

            do {
                buffer[i++] = c;
            }
            while ((ret = read(fd, &c, 1) > 0) && c != del);
                
            if (ret < 0 || ret == 0) {
                free(buffer);
                return NULL;
            }
        }

        if (c != '\r') {
            buffer[i++] = c;
        }
    }

    buffer[size] = '\0';
    return buffer;
}

/* Starter function of the table. It opens the table and initializes the table fields.*/
int table_start(char* file) {
    if ((table.fd = open(file, O_RDWR, READ_PERMS | WRITE_PERMS)) < 0)
        return -1;

    // Initializes ncolumn and nrow variables.
    if ((rc_stats(table.fd, &table.nrows, &table.ncolumn)) < 0 )
        return -1;
    
    table.started = 1;    
    
    active_reader = 0;
    active_writer = 0;
    waiting_reader = 0;
    waiting_writer = 0;
    
    if ((pthread_cond_init(&okToRead, NULL) != 0 || pthread_cond_init(&okToWrite, NULL) != 0) ||
        pthread_mutex_init(&rw_lock, NULL) != 0 ) {
        return -1;
    }

    return 0;
}

/* End function of the table. It frees all alocated resources and destroyes mutexes and condition variables. */
void table_end() {
    if (table.table != NULL) {
        for (size_t i = 0; i < table.ncolumn; i++) {
            if (table.table[i]) {
                for (size_t j = 0; j < table.nrows; j++) {
                    if (table.table[i][j]) free(table.table[i][j]);
                }
            free(table.table[i]);
            }
        }
        free(table.table);
    }
    if (table.fd > 0) close(table.fd);
    table.fd = -1;
    
    pthread_cond_destroy(&okToRead);
    pthread_cond_destroy(&okToWrite);
    pthread_mutex_destroy(&rw_lock);
}

// Reads a single column entry from a row of the csv file.
char* read_single_enrty(char* buffer, int* offset) {
    char* entry;
    int size = 0;

    // Check size of the entry
    for (size_t i = *offset; i < strlen(buffer) && (buffer[i] != ',' && buffer[i] != '\n' && buffer[i] != '\0'); i++) {
        size++;

        if (buffer[i] == '\"') {
            do {
                i++;
                size++;
            } while (i < strlen(buffer) && buffer[i] != '\"');
        }
    }

    if ((entry = malloc(sizeof(char) * (size+1))) == NULL)
        return NULL;

    // Set the entry
    memset(entry, 0, size+1);
    for (size_t i = *offset, j = 0; j < size; i++, j++) {
        entry[j] = buffer[i];
    }

    *offset += size+1;
    return entry;
}

/* Allocate space from the memory for the table. */
int init_memories() {
    if (!table.started) 
        return -1;

    if ((table.table = (char***)malloc(sizeof(char**)*table.ncolumn)) == NULL)
        return -1;
    
    for (size_t i = 0; i < table.ncolumn; i++) {
        if ((table.table[i] = malloc(sizeof(char*)*table.nrows)) == NULL)
            return -1;
    }
    return 0;
}

/* Load the data from the csv file into the main memory. */
int table_load() {
    if (!table.started ||
        lseek(table.fd, 0, SEEK_SET) < 0 ||
        init_memories() == -1) 
        return -1;

    char* buffer = NULL;

    for (size_t j = 0; j < table.nrows; j++) {
        // Read a line from the file
        if ((buffer = readnextline(table.fd)) == NULL) // read the column names
            return -1; 
        
        // Read column fields
        int offset = 0;
        for (size_t i = 0; i < table.ncolumn; i++) {
            table.table[i][j] = read_single_enrty(buffer, &offset);
            
            if (table.table[i][j] == NULL) {
                free(buffer);
                return -1;
            }
        }
        
        free(buffer);
    }

    return 0;
}

/*  FUNCTIONS THAT ARE USED TO PROCESS QUERIES*/

/* Returns given columns from the table by discarding the given rows
 * columns: Array that includes the columns that will be selected.
 * discard_table: Array that includes the rows that are not going to be returned.
*/
char* select_from_table(long* columns, int no_columns, long* discard_table, long* records) {
    char* str = NULL;
    int str_size = 1;

    // Calculate size of the table that will be converted to a message and returned.
    for (size_t i = 0; i < no_columns; i++) 
    {
        for (size_t j = 0; j < table.nrows; j++) 
        {
            if (discard_table == NULL || discard_table[j] == -1) {
                str_size += strlen(table.table[columns[i]][j])+1;  //Column_Entry\t or Column_Entry\n
            }
        }
    }
    
    if ((str = (char*)malloc(sizeof(char)*str_size)) == NULL)
        return NULL;

    memset(str, 0, str_size);

    // Copy the records to the output array.
    for (size_t j = 0, first = 1; j < table.nrows; j++) 
    {
        // Check if the row is discarded
        if (discard_table == NULL || discard_table[j] == -1) 
        {   
            ++(*records);
            for (size_t i = 0; i < no_columns; i++) 
            {
                //If it is the first entry;
                if (first) {     
                    first = 0;
                    strcpy(str, table.table[columns[i]][j]);
                } else {
                    strcat(str, table.table[columns[i]][j]);
                }

                if (i+1 == no_columns) {
                    strcat(str, "\n");
                } else {
                    strcat(str, ",");
                }
            }
        }
    }
    return str;
}

// Finds the column (c) index from the table
long column_index(char* c) {
    if (c == NULL)
        return -1;

    for (size_t i = 0; i < table.ncolumn; i++) {
        if (strcmp(table.table[i][0], c) == 0) {
            return i;
        }
    }
    return -1;
}

/* Parses the given query and finds the index of the columns that are presented in the query. */
long column_array(long* select_i, char* query) {
    char* save, *command = strtok_r(query, ", ", &save);
    long no_colmns = 0, ci;
    
    // Parse the tokens
    size_t i = 0;
    while (command != NULL && strcmp(command, "FROM") != 0) {
        
        // Find index of the column
        if ((ci = column_index(command)) < 0) {
            return -1;
        }

        select_i[i++] = ci; 
        ++no_colmns;
        command = strtok_r(NULL, ", ", &save);
    }

    return no_colmns;
}

/* Checks if the next token of the query is '*' */
int check_star(char* query) {
    for (size_t i = 0; i < strlen(query);) {
        if (query[i] == ' ') 
        {
            i++;
        }
        else if (query[i] == '*') 
        {
            return 1;
        } else 
        {
            return 0;
        }
    }
    return -1;
}

/* General function to parse the queries that includes UPDATE keyword */
char* UPDATE(char* query, long *records) {
    if (query == NULL)
        return NULL;
    
    char *save, *command = strtok_r(query, " ", &save);
    command = strtok_r(NULL, " ", &save);
    
    if (strcmp(command, "SET") != 0) {
           return NULL;
    }

    int c;
    char **values;
    long *update_i;

    if ((values = malloc(sizeof(char*)*table.ncolumn)) == NULL || 
        (update_i = malloc(sizeof(long)*table.ncolumn)) == NULL) {
        return NULL;
    }

    size_t count = 0;
    
    // Parse the query columns that are going to be updated
    while ((command = strtok_r(NULL, " =',", &save)) && strcmp(command, "WHERE") != 0) {
        // Find the column index
        if ((c = column_index(command)) == -1 ||
            (command = strtok_r(NULL, "'", &save)) == NULL
        ) {
            free(values);
            free(update_i);
            return NULL;
        }

        // Store the column indexes
        update_i[count] = c;
        values[count] = command;
        ++count;
    }

    // Parse the query column that comes after WHERE keyword and represents the columns that are going to be changed.
    if ((command = strtok_r(NULL, " =',", &save)) == NULL ||    // Column to replace
        (c = column_index(command)) == -1 || 
        (command = strtok_r(NULL, "'", &save)) == NULL)      // Value of the column
    { 
        free(values);
        free(update_i);
        return NULL;
    }
    
    // Update table fields
    for (size_t i = 0; i < table.nrows; i++) {
        if (strcmp(table.table[c][i], command) == 0) {
            ++(*records);
            for (size_t j = 0; j < count; j++) {
                free(table.table[update_i[j]][i]);
                if ((table.table[update_i[j]][i] = strdup(values[j])) == NULL) {
                        free(values);
                        free(update_i);
                        return NULL;
                }
            }
        }
    }
    
    free(values);
    free(update_i);
    
    return "SUCCESS\n";
}

/* General function to parse the queries that includes SELECT keyword */
char* SELECT(char* query, long *records) {
    if (query == NULL)
        return NULL;

    long *select_i, no_colmns;

    if ((select_i = malloc(sizeof(long)*table.ncolumn)) == NULL)
        return NULL;
    
    memset(select_i, 0, table.ncolumn);
    
    // Here, Column array will be filled according to parsed column names.
    // The column indexes that are included in column array will be returned from the table.
    
    // Check if the next token is '*'.
    if (check_star(query)) {
        for (size_t i = 0; i < table.ncolumn; i++) { select_i[i] = i; }
        no_colmns = table.ncolumn;
    }
    else  {   
        if ((no_colmns = column_array(select_i, query)) <= 0 ) {
            return NULL;
        }
    }

    char* ret = select_from_table(select_i, no_colmns, NULL, records);
    free(select_i);

    return ret;
}

/* General function to parse the queries that includes SELECT and DISTINCT keyword */
char* SELECT_DISTINCT(char* query, long *records) {
    if (query == NULL)
        return NULL;
    
    char *save, *command = strtok_r(query, " ", &save);
    long *select_i, *discard_table, no_colmns;
    
    if (command == NULL || strcmp(command, "DISTINCT") != 0)
        return NULL;

    if ((select_i = malloc(sizeof(long)*table.ncolumn)) == NULL)
        return NULL;

    // Here, Column array will be filled according to parsed column names.
    // The column indexes that are included in column array will be returned from the table.
    if ((no_colmns = column_array(select_i, save)) <= 0) {
        free(select_i);
        return NULL;
    }

    if ((discard_table = malloc(sizeof(long)*(table.nrows))) == NULL) {
        free(select_i);
        return NULL;
    }

    for (size_t i = 0; i < table.nrows; i++) { discard_table[i] = -1;}

    // Load the discard table. If the rows are duplicated, discard table will be marked accordingly.
    for (size_t i = 1; i < table.nrows-1; i++) 
    {
        if (discard_table[i] == -1) {
            for (size_t j = i+1; j < table.nrows; j++)
            {
                if (discard_table[j] == -1) {
                    int dup = 1;
                    for (size_t k = 0; k < no_colmns; k++) {
                        if (strcmp(table.table[select_i[k]][i], table.table[select_i[k]][j]) != 0) {
                            dup = 0;
                        }
                    }
                    if (dup == 1) {
                        discard_table[j] = 1;
                    }
                }
            }
        }
    }

    // Return the table according to column array and discard table.
    char* ret = select_from_table(select_i, no_colmns, discard_table, records);
    
    free(discard_table);
    free(select_i);
    return ret;
}

// Check if the next token of the query is 'DISTINCT'
int check_distict(char* query) {
    char* key = "DISTINCT";

    for (size_t i = 0; i < strlen(key); i++) {
        if (query[i] != key[i])
            return 0;
    }
    return 1;
}

/* Main query parser function. According to the first token, the main function is detected.
 * KEYWORDS: UPDATE, SELECT, SELECT DISTINCT
*/
char* query_parser(char* query, long *records) {
    if (query == NULL)
        return NULL;
    
    char *ret, *save, *command = strtok_r(query, " ", &save);
    
    if (strcmp(command, "SELECT") == 0) {
        reader_start();
        
        if (check_distict(save))  {    
            ret = SELECT_DISTINCT(save, records);
        } 
        else {
            ret = SELECT(save, records);
        }
        
        reader_end();
        return ret;
    }
    else if (strcmp(command, "UPDATE") == 0) {
        writer_start();
        ret = UPDATE(save, records);
        writer_end();

        return ret;
    }
    
    return NULL;
}
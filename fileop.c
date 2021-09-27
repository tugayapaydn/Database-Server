#include "include/fileop.h"

int line_count(int fd) {
    char c;
    int line = 1;
    int ret;

    if (lseek(fd, 0, SEEK_SET) < 0) 
        return -1;

    char prev;
    while ((ret = read(fd, &c, 1)) > 0) {
        if (c == '\n')
            line++;
        
        prev = c;
    }

    if (ret < 0)
        return -1;

    if (prev == '\n')
        --line;
        
    return line;
}

int* init_line_stats(int fd, int* size) {
    char c;
    int ret;
    int line;
    int lcnt;
    int* arr;

    line = 1;
    lseek(fd, 0, SEEK_SET);
    while ((ret = read(fd, &c, 1)) > 0) {
        if (c == '\n')
            line++;
    }

    if ((arr = malloc(sizeof(int)*line)) == NULL) {
        return NULL;
    }
    *size = line;
    
    lseek(fd, 0, SEEK_SET);

    line = 0;
    lcnt = 0;
    while ((ret = read(fd, &c, 1)) > 0) {
        if (c == '\n') {
            arr[line] = lcnt;
            lcnt = 0;
            line++;
        }
        else {
            lcnt++;
        }
    }
    arr[line] = lcnt;

    lseek(fd, 0, SEEK_SET);
    return arr;
}

/***************************************************************
 * Function finds the starting offset of the line
 * Inputs: 
 *  fd   = File descriptor
 *  line = Line number to be searched
 * 
 * Output:
 *  If successful, it returns the offset; otherwise -1
****************************************************************/
off_t get_line_offset(int fd, int line) {
    if (line < 0)
        return -1;

    if (line == 0)
        return 0;

    off_t offset = 0;
    int currLine = 0;
    char c;

    if (lseek(fd, 0, SEEK_SET) == -1) 
        return -1;

    while (read(fd, &c, 1) > 0) {
        if ((c == '\n') && (++currLine == line))
            return offset+1;
        
        offset++;
    }
    return -1;
}

/*************************************************************************
 * Function reads the line specified by line variable. 
 * Inputs: 
 *  fd   = File descriptor
 *  line = Line number to be read
 * 
 * Output:
 *  Line    = If successful, it returns the line (char*); otherwise NULL
 *  offset  = Set starting index of the line to offset variable.
**************************************************************************/
char* readline(int fd, int line, off_t* offset) {
    char* buffer;
    char c;
    int size = 0;

    if ((*offset = get_line_offset(fd, line)) == -1)
        return NULL;

    if (lseek(fd, *offset, SEEK_SET) == -1)
        return NULL;

    while (read(fd, &c, 1) > 0 && c != '\n') { size++; }
    
    if (lseek(fd, *offset, SEEK_SET) == -1)
        return NULL;
    
    if ((buffer = ((char*)malloc(size+1))) == NULL) 
        return NULL;

    for (size_t i = 0; i < size+1; i++) { buffer[i] = '\0'; }

    if (lseek(fd, *offset, SEEK_SET) == -1) {
        free(buffer);
        return NULL;
    }
    
    if (read(fd, buffer, size) == -1) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

char* readnextline(int fd) {
    char c;
    char* buffer;
    int size = 0;
    
    off_t offset = lseek(fd, 0, SEEK_CUR);

    while (read(fd, &c, 1) > 0 && c != '\n') { size++; }
    if (size == 0) 
        return NULL;

    if (lseek(fd, offset, SEEK_SET) == -1)
        return NULL;
    
    if ((buffer = ((char*)malloc(size+1))) == NULL) 
        return NULL;

    for (size_t i = 0; i < size+1; i++) { buffer[i] = '\0'; }

    if (read(fd, buffer, size+1) == -1) {
        free(buffer);
        return NULL;
    }
    buffer[size] = '\0';
    return buffer;
}

char* sreadnextline(int fd, int size) {
    char* buffer;
    char t;

    if ((buffer = ((char*)malloc(size+1))) == NULL)
        return NULL;

    for (size_t i = 0; i < size+1; i++) { buffer[i] = '\0'; }

    if (read(fd, buffer, size) < 0) {
        free(buffer);
        return NULL;
    }

    if (read(fd, &t, 1) < 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

/*************************************************************************
 * Function inserts the buffer to the pos index in the file fd
 * Inputs: 
 *  fd      = File descriptor
 *  buffer  = String to be inserted to the file
 *  size    = Size of the buffer
 *  pos     = The offset that the buffer will be inserted.
 * 
 * Output:
 *  0 if successful, otherwise -1
**************************************************************************/
int insert_to_file(int fd, char* buffer, int size, off_t pos) {
    if (pos < 0 || buffer == NULL || size <= 0)
        return -1;

    struct stat st;
    char* text_bc = NULL;
    int text_bc_size;
    off_t total_size;

    if (fstat(fd, &st) == -1)
        return -1;
    
    total_size = st.st_size;
    
    if (lseek(fd, pos, SEEK_SET) == -1)
        return -1;

    text_bc_size = total_size-pos+1;
    if ((text_bc = malloc(text_bc_size)) == NULL)
        return -1;
    
    for (size_t i = 0; i < text_bc_size; i++) { text_bc[i] = '\0'; }
    
    if (read(fd, text_bc, text_bc_size-1) == -1) {
        free(text_bc);
        return -1;
    }
    text_bc[text_bc_size-1] = '\0';

    if (lseek(fd, pos, SEEK_SET) == -1) {
        free(text_bc);
        return -1;
    }

    if (write(fd, buffer, size) == -1) {
        free(text_bc);
        return -1;
    }

    if (write(fd, text_bc, text_bc_size-1) == -1) {
        free(text_bc);
        return -1;
    }
    
    free(text_bc);
    return 0;
}
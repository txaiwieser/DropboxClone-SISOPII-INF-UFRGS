#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include "../include/dropboxUtil.h"

pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;

// Debug messages, displayed only when DEBUG=1
void debug_printf(const char* message, ...) {
    if(DEBUG) {
        va_list args;
        va_start(args, message);
        vprintf(message, args);
        va_end(args);
    }
}

/* Create dir if it doesn't exist
   Return:   0: created dir
            -1: dir exists or there was an error when trying to create it
 */
int makedir_if_not_exists(const char* path) {
    struct stat st = {0};
    int result = -1;
    pthread_mutex_lock(&fileMutex);
    if (stat(path, &st) == -1) {
        result = mkdir(path, 0700);
    }
    pthread_mutex_unlock(&fileMutex);
    return result;
}

// If file exists and is a file (not a directory)
int file_exists(const char* path) {
    struct stat st = {0};
    pthread_mutex_lock(&fileMutex);
    int stat_result = (stat(path, &st) == 0) && S_ISREG(st.st_mode);
    pthread_mutex_unlock(&fileMutex);
    return stat_result;
}

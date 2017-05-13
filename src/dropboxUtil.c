#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "../include/dropboxUtil.h"

/* Mensagens de debug */
void debug_printf(const char* message, ...) {
    if(DEBUG) {
        va_list args;
        va_start(args, message);
        vprintf(message, args);
        va_end(args);
    }
}

void makedir_if_not_exists(const char* path){
  struct stat st = {0};
  if (stat(path, &st) == -1) {
      mkdir(path, 0700);
  }
}


int isValidFilename(char *filename){
    /* Remove spaces from filename */
    int count = 0;
    // Traverse the given string. If current character
    // is not space, then place it at index 'count++'
    for (int i = 0; str[i]; i++)
        if (str[i] != ' ')
            str[count++] = str[i]; // here count is
                                   // incremented
    str[count] = '\0';


    // TODO check if filename exists

    // TODO return? 

    return 0;
}

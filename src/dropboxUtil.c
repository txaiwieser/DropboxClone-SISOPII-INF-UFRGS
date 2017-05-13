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
    // TODO filenames can have spaces?
    /* Remove spaces from filename */
    int count = 0;
    for (int i = 0; filename[i]; i++)
        if (filename[i] != ' ')
            filename[count++] = filename[i];
    filename[count] = '\0';


    // TODO check if filename exists

    // TODO return?

    return 0;
}

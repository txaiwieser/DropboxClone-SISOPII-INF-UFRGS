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

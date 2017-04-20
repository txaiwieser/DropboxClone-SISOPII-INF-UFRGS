#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
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

#define MAXNAME 255

#define DEBUG 1

void debug_printf(const char* message, ...);

void makedir_if_not_exists(const char* path);

int dir_exists(const char* path);

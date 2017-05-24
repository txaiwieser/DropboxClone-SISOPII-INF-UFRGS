#define MAXNAME 255
#define METHODSIZE 256
#define DEBUG 1

#define MIN(a,b) (a < b)? a : b

void debug_printf(const char* message, ...);

void makedir_if_not_exists(const char* path);

int dir_exists(const char* path);

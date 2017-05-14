#define MAXNAME 50 // REVIEW Quanto deve ser? Pedir pro monitor

#define DEBUG 1

void debug_printf(const char* message, ...);

void makedir_if_not_exists(const char* path);

int dir_exists(const char* path);

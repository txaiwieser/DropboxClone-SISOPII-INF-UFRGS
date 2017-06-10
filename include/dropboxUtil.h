#define DEBUG 1 // Enable (1) or disable (0) debug messages
#define MAXNAME 255 // Maximum filename length
#define METHODSIZE 255 // Method messages (DOWNLOAD filename, UPLOAD filename, PUSH filename, etc) length
#define INVALIDSOCKET -1

#define MIN(a,b) (a < b)? a : b

void debug_printf(const char* message, ...);
void makedir_if_not_exists(const char* path);
int connect_server(char * host, int port);
int dir_exists(const char* path);
int file_exists(const char* path);

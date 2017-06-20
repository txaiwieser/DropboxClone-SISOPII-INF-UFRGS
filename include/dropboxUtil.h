#define DEBUG 1 // Enable (1) or disable (0) debug messages
#define MAXNAME 255 // Maximum filename length
#define MAXFILES 200 // Maximum number of files in user dir
#define METHODSIZE 255 // Method messages (DOWNLOAD filename, UPLOAD filename, PUSH filename, etc) length

// Both constants must have TRANSMISSION_MSG_SIZE characteres!
#define TRANSMISSION_CONFIRM "OK"
#define TRANSMISSION_CANCEL "ER"
#define TRANSMISSION_MSG_SIZE 2

#define MIN(a,b) (a < b)? a : b

void debug_printf(const char* message, ...);
int makedir_if_not_exists(const char* path);
int connect_server(char * host, int port);
int dir_exists(const char* path);
int file_exists(const char* path);

#include "dropboxUtil.h"

/* Struct used in list of ignored files */
struct tailq_entry {
    char filename[MAXNAME];
    TAILQ_ENTRY(tailq_entry) entries;
};

// Inotify constants
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

int getTimeServer(void);
void sync_client();
void send_file(char *file);
void get_file(char *file, char *path);
void delete_server_file(char *file);
void close_connection();

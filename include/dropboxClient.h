#include "dropboxUtil.h"

int connect_server(char *host, int port);
void sync_client();
void send_file(char *file);
void get_file(char *file, char *path);
void delete_server_file(char *file);
void close_connection();

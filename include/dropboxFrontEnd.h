#include "dropboxUtil.h"

typedef struct replication_server {
  char ip[20];
  int port;
} REPLICATION_SERVER_t;

void *client_server_handler();

typedef struct replication_server {
  char ip[20];
  int port;
  int isAvailable;
  SSL* socket;
} REPLICATION_SERVER_t;

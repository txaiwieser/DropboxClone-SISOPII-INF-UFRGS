#include "dropboxUtil.h"

typedef struct client {
  pthread_mutex_t mutex; // user mutex
  SSL *devices[MAXDEVICES]; // associado aos dispositivos do usuário
  SSL *devices_server[MAXDEVICES]; // socket 'servidor' do cliente, que recebe requisições de PUSH e DELETE
  char userid[MAXNAME]; // id do usuário no servidor, que deverá ser único. Informado pela linha de comando.
  FILE_INFO_t file_info[MAXFILES];  // metadados de cada arquivo que o cliente possui no servidor
  int logged_in;  // cliente está logado ou não
} CLIENT_t;

/* Struct used in list of clients */
struct tailq_entry {
    CLIENT_t client_entry;
    TAILQ_ENTRY(tailq_entry) entries;
};

void sync_server();

void receive_file(char *file);

void send_file(char *file);

void delete_file(char *file);

void *connection_handler(void *socket_desc);

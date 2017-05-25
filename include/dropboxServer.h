#include "dropboxUtil.h"

#define MAXFILES 200
#define MAXDEVICES 2

#define FREE_FILE_SIZE -1

typedef struct file_info {
  char name[MAXNAME]; // refere-se ao nome do arquivo, incluindo a extensão
  time_t last_modified; // refere-se a data da última modificação no arquivo
  int size; // indica o tamanho do arquivo, em bytes
} FILE_INFO_t;

typedef struct client {
  int devices[MAXDEVICES]; // associado aos dispositivos do usuário
  int devices_server[MAXDEVICES]; // socket 'servidor' do cliente  // REVIEW explicar isso melhor
  char userid[MAXNAME]; // id do usuário no servidor, que deverá ser único. Informado pela linha de comando.
  FILE_INFO_t file_info[MAXFILES];  // metadados de cada arquivo que o cliente possui no servidor
  int logged_in;  // cliente está logado ou não
} CLIENT_t;

struct tailq_entry{
    CLIENT_t client_entry;
    TAILQ_ENTRY(tailq_entry) entries;
};

void sync_server();

void receive_file(char *file);

void send_file(char *file);

void delete_file(char *file);

void *connection_handler(void *socket_desc);

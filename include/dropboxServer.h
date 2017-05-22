#include "dropboxUtil.h"

#define MAXFILES 100

typedef struct file_info {
  char name[MAXNAME]; // refere-se ao nome do arquivo
  char extension[MAXNAME]; // refere-se ao tipo de extensão do arquivo
  char last_modified[MAXNAME]; // refere-se a data da última modificação no arquivo
  int size; // indica o tamanho do arquivo, em bytes
} FILE_INFO_t;

typedef struct client {
  int devices[2]; // associado aos dispositivos do usuário
  char userid[MAXNAME]; // id do usuário no servidor, que deverá ser único. Informado pela linha de comando.
  FILE_INFO_t file_info[MAXFILES];  // metadados de cada arquivo que o cliente possui no servidor
  int logged_in;  // cliente está logado ou não
} CLIENT_t;

struct tailq_entry{
    struct client client_entry;
    TAILQ_ENTRY(tailq_entry) entries;
};

void sync_server();

void receive_file(char *file);

void send_file(char *file);

void *connection_handler(void *socket_desc);

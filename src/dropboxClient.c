#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../include/dropboxClient.h"

/* Para ativar as mensagens de debug, defina como 1 o valor da constante abaixo */
#define DEBUG 1

/* Mensagens de debug */
void debug_printf(const char* message, ...) {
    if(DEBUG) {
        va_list args;
        va_start(args, message);
        vprintf(message, args);
        va_end(args);
    }
}

// usage : ./dropboxClient fulano endereço porta

// METHODS:
// upload <path/filename.ext>
// download <filename.ext>
// list
// get_sync_dir
// exit

void cmdUpload(){
  printf("-> UPLOAD\n");
};

void cmdDownload(){
  printf("-> DOWNLOAD\n");
};

void cmdList(){
  printf("-> LIST\n");
};

void cmdGetSyncDir(){
  printf("-> GET SYNC DIR\n");
};

void cmdExit(){
  printf("-> EXIT\n");
};

void cmdMan(){
  printf("-> HELP. Available commands:\n");
  printf("upload <path/filename.ext>\n");
  printf("download <filename.ext>\n");
  printf("list\n");
  printf("get_sync_dir\n");
  printf("exit\n");
  printf("help\n");
};

int main(int argc, char *argv[]){
  if (argc < 4) { // Número incorreto de parametros
          printf("Usage: %s fulano endereço porta\n", argv[0]);
          return 1;
  }
  debug_printf("~~ BEGIN ~~\nUsuario: %s\nEndereco: %s\nPorta: %s\n", argv[1], argv[2], argv[3]);

    char cmd[256];
    char *token;

    printf ("Welcome to Dropbox! - v 1.0\n");
    cmdMan();

    while (1) {
        printf ("Dropbox> ");
        gets(cmd);
        if( (token = strtok(cmd," \t")) != NULL ) {
            if (strcmp(token,"exit")==0) {
                cmdExit();
                break;
            }
            else if (strcmp(token,"upload")==0) cmdUpload();
            else if (strcmp(token,"download")==0) cmdDownload();
            else if (strcmp(token,"list")==0)  cmdList();
            else if (strcmp(token,"get_sync_dir")==0) cmdGetSyncDir();
            else if (strcmp(token,"help")==0) cmdMan();
            else printf ("Invalid command! Type 'help' to see the available commands\n");
        }
    }

    return 0;
}

int connect_server(char *host, int port){
    return 0;
}

void sync_client(){

}

void send_file(char *file){

}

void get_file(char *file){

}

void close_connection(){

}

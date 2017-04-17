#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// TODO criar makefile, .gitignore
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

int main(){
  // TODO get argc and argv
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

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

char server_host[50]; // TODO max host name length?
int server_port = 0;

void cmdUpload(){
        printf("-> UPLOAD\n");
};

void cmdDownload(){
        //char *download_test_method = "DOWNLOAD filename.ext\0"; // TODO fazer funcionar sem esse \0 ou inclui-lo na hora de enviar pro servidor?
        printf("-> DOWNLOAD\n");
};

void cmdList(){
        char *method = "LIST";
        int valread;
        char buffer[1024] = {0};

        int sock = connect_server(server_host, server_port);
        debug_printf("[Socket number %d]\n", sock);
        // TODO verificar valor do sock se é > 0?

        send(sock, method, strlen(method), 0 );
        debug_printf("[%s method sent]\n", method);
        valread = read( sock, buffer, 1024);
        printf("Files: %s\n", buffer );
        // TODO close_connection();
};

void cmdGetSyncDir(){
};

void cmdMan(){
        printf("\nAvailable commands:\n");
        printf("get_sync_dir\n");
        printf("list\n");
        printf("download <filename.ext>\n");
        printf("upload <path/filename.ext>\n");
        printf("help\n\n");
        printf("exit\n");
};

int main(int argc, char *argv[]){
        char cmd[256];
        char *token;

        if (argc < 4) { // Número incorreto de parametros
                printf("Usage: %s <user> <IP> <port>\n", argv[0]);
                return 1;
        }
        debug_printf("[Client started with parameters User=%s IP=%s Port=%s]\n", argv[1], argv[2], argv[3]);
        strcpy(server_host, argv[2]);
        server_port = atoi(argv[3]);

        printf ("Welcome to Dropbox! - v 1.0\n");
        cmdMan();

        while (1) {
                printf ("Dropbox> ");
                gets(cmd);
                if( (token = strtok(cmd," \t")) != NULL ) {
                        if (strcmp(token,"exit")==0) {
                                break;
                        }
                        else if (strcmp(token,"upload")==0) cmdUpload();
                        else if (strcmp(token,"download")==0) cmdDownload();
                        else if (strcmp(token,"list")==0) cmdList();
                        else if (strcmp(token,"get_sync_dir")==0) cmdGetSyncDir();
                        else if (strcmp(token,"help")==0) cmdMan();
                        else printf ("Invalid command! Type 'help' to see the available commands\n");
                }
        }

        return 0;
}

int connect_server(char *host, int port){
        struct sockaddr_in address;
        int sock = 0;
        struct sockaddr_in serv_addr;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                printf("\n Socket creation error \n");
                return -1;
        }

        memset(&serv_addr, '0', sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
        {
                printf("\nInvalid address/ Address not supported \n");
                return -1;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
                printf("\nConnection Failed \n");
                return -1;
        }

        return sock;
}

void sync_client(){

}

void send_file(char *file){

}

void get_file(char *file){

}

void close_connection(){

}

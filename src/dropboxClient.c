#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

char server_host[50]; // TODO max host name length? or use malloc!
int server_port = 0, sock = 0;
char *server_user[50]; // TODO what's the max username? or use malloc!

void cmdUpload(char *filename) {
    // TODO verificar se foi passado um nome de arquivo válido (nao vazio?)
    // TODO fazer funcionar se houver espaços a mais antes do filename (e pode ser que haja depois também...)
    int valread;
    char buffer[1024] = {0};

    // Concatenate strings to get method = "DOWNLOAD filename"
    char * method =  malloc(7+strlen(filename)+1);
    strcpy(method,"UPLOAD ");
    strcat(method, filename);

    send(sock, method, strlen(method), 0);
    debug_printf("[%s method sent]\n", method);
    valread = read(sock, buffer, 1024);

    //printf("Uploading file %s\n", filename) // TODO show progress?
    // TODO upload file using send_file()
};

void cmdDownload(char *filename) {
    // TODO verificar se foi passado um nome de arquivo válido (nao vazio?)
    // TODO fazer funcionar se houver espaços a mais antes do filename (e pode ser que haja depois também...)
    int valread;
    char buffer[1024] = {0};

    // Concatenate strings to get method = "DOWNLOAD filename"
    char * method =  malloc(9+strlen(filename)+1);
    strcpy(method,"DOWNLOAD ");
    strcat(method, filename);

    send(sock, method, strlen(method), 0);
    debug_printf("[%s method sent]\n", method);
    valread = read(sock, buffer, 1024);

    //printf("Downloading file %s\n", filename) // TODO show progress?
    // TODO download file using get_file()
};

void cmdList() {
    char * method = "LIST";
    int valread;
    char buffer[1024] = {0};

    send(sock, method, strlen(method), 0);
    debug_printf("[%s method sent]\n", method);
    valread = read(sock, buffer, 1024);
    printf("Files: \n%s\n", buffer);

    // TODO suportar uma lista "infinita" de arquivos
};

void cmdGetSyncDir() {
    struct stat st = {0};
    char *folder = malloc(strlen(getenv("HOME"))+10+strlen(server_user)+1);
    strcpy(folder,getenv("HOME"));
    strcat(folder,"/sync_dir_");
    strcat(folder, server_user);

    // Create folder if it doesn't exist
    if (stat(folder, &st) == -1) {
        mkdir(folder, 0700);
        printf("Created folder %s\n", folder);
    } else {
        printf("Folder already exists\n");
    }
};

void cmdMan() {
    printf("\nAvailable commands:\n");
    printf("get_sync_dir\n");
    printf("list\n");
    printf("download <filename.ext>\n");
    printf("upload <path/filename.ext>\n");
    printf("help\n");
    printf("exit\n\n");
};

void cmdExit(){
  close_connection();
}

int main(int argc, char * argv[]) {
    char cmd[256];
    char filename[256];
    char * token;

    if (argc < 4) { // Número incorreto de parametros
        printf("Usage: %s <user> <IP> <port>\n", argv[0]);
        return 1;
    }
    debug_printf("[Client started with parameters User=%s IP=%s Port=%s]\n", argv[1], argv[2], argv[3]);
    strcpy(server_host, argv[2]);
    strcpy(server_user, argv[1]);
    server_port = atoi(argv[3]);

    sock = connect_server(server_host, server_port);
    if (sock < 0) {
        return -1;
    }
    debug_printf("[Connection established. Socket number %d]\n", sock);

    printf("Welcome to Dropbox! - v 1.0\n");
    cmdMan();

    while (1) {
        printf("Dropbox> ");
        gets(cmd);
        if ((token = strtok(cmd, " \t")) != NULL) {
            if (strcmp(token, "exit") == 0) {
                break;
            } else if (strcmp(token, "upload") == 0) {
              strcpy(filename, cmd+7);
              cmdUpload(filename);
            } else if (strcmp(token, "download") == 0) {
                strcpy(filename, cmd+9);
                cmdDownload(filename);
            }
            else if (strcmp(token, "list") == 0) cmdList();
            else if (strcmp(token, "get_sync_dir") == 0) cmdGetSyncDir();
            else if (strcmp(token, "help") == 0) cmdMan();
            else printf("Invalid command! Type 'help' to see the available commands\n");
        }
    }

    return 0;
}

int connect_server(char * host, int port) {
    struct sockaddr_in address;
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset( & serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", & serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed. \n");
        return -1;
    }

    return sock;
}

void sync_client() {

}

void send_file(char * file) {

}

void get_file(char * file) {

}

void close_connection() {
  shutdown(sock, 2); // Stop both reception and transmission.
}

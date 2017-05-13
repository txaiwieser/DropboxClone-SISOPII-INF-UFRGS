#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

char server_host[256];
int server_port = 0, sock = 0;
char server_user[MAXNAME];

void send_file(char *file) {
    // TODO verificar se foi passado um nome de arquivo válido (nao vazio? existente?)
    // TODO fazer funcionar se houver espaços a mais antes do filename (e pode ser que haja depois também...)
    int valread;
    char buffer[1024] = {0};
    char method[160];

    // Concatenate strings to get method = "UPLOAD filename"
    sprintf(method, "UPLOAD %s", file);

    // Call to the server
    send(sock, method, strlen(method), 0);
    debug_printf("[%s method sent]\n", method);
    valread = read(sock, buffer, 1024);

    //printf("Uploading file %s\n", filename)
};

void get_file(char *file) {
    // TODO verificar se foi passado um nome de arquivo válido (nao vazio? existente?)
    // TODO fazer funcionar se houver espaços a mais antes do filename (e pode ser que haja depois também...)
    int valread;
    char buffer[1024] = {0};
    char method[160];

    // Concatenate strings to get method = "DOWNLOAD filename"
    sprintf(method, "DOWNLOAD %s", file);

    // Send to the server
    send(sock, method, strlen(method), 0);
    debug_printf("[%s method sent]\n", method);
    valread = read(sock, buffer, 1024);

    //printf("Downloading file %s\n", filename)
};

void cmdList() {
    int valread, nLeft;
    char buffer[1024] = {0};

    send(sock, "LIST", 4, 0);

    // Receive length
    read(sock, &nLeft, sizeof(nLeft));
    nLeft = ntohl(nLeft);

    /* Receive data in chunks */
    while(nLeft > 0 && (valread = read(sock, buffer, sizeof(buffer))) > 0){
      buffer[valread] = '\0';
      printf("%s", buffer);
      nLeft -= valread;
    }
};

void cmdGetSyncDir() {
    char user_sync_dir_path[256];

    // Define path to user sync_dir folder
    sprintf(user_sync_dir_path, "%s/sync_dir_%s", getenv("HOME"), server_user);

    // Create user sync_dir if it doesn't exist
    makedir_if_not_exists(user_sync_dir_path);
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

    // Check number of parameteres
    if (argc < 4) {
        printf("Usage: %s <user> <IP> <port>\n", argv[0]);
        return 1;
    }

    // Connect to server
    debug_printf("[Client started with parameters User=%s IP=%s Port=%s]\n", argv[1], argv[2], argv[3]);
    strcpy(server_host, argv[2]);
    strcpy(server_user, argv[1]);
    server_port = atoi(argv[3]);
    sock = connect_server(server_host, server_port);
    if (sock < 0) {
        return -1;
    }
    debug_printf("[Connection established. Socket number %d]\n", sock);

    // Send username to server
    write(sock, server_user, strlen(server_user));

    printf("Welcome to Dropbox! - v 1.0\n");
    cmdMan();

    // Handle user input
    while (1) {
        printf("Dropbox> ");
        // TODO tratar caso usuário apenas dê um enter
        scanf(" %[^\t\n]s",&cmd);
        if ((token = strtok(cmd, " \t")) != NULL) {
            if (strcmp(token, "exit") == 0) break;
            else if (strcmp(token, "upload") == 0) {
              strcpy(filename, cmd+7);
              send_file(filename);
            }
            else if (strcmp(token, "download") == 0) {
                //strcpy(filename, cmd+9);
                sprintf(filename, "%s", cmd+9);
                removeSpaces(&filename);
                printf("filename=%s\n", filename);
                get_file(filename);
            }
            else if (strcmp(token, "list") == 0) cmdList();
            else if (strcmp(token, "get_sync_dir") == 0) cmdGetSyncDir();
            else if (strcmp(token, "help") == 0) cmdMan();
            else printf("Invalid command! Type 'help' to see the available commands\n");
        } else { printf("aquii"); }
    }

    return 0;
}

int connect_server(char * host, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    server = gethostbyname(host);
    if (server == NULL) {
        printf("ERROR, no such host\n");
        return -1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset( & serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

    if (connect(sock, (struct sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed. \n");
        return -1;
    }

    return sock;
}

void sync_client() {

}

void close_connection() {
    // Stop both reception and transmission.
    shutdown(sock, 2);
}

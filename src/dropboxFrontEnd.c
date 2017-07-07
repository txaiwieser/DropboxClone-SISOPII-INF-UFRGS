#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../include/dropboxFrontEnd.h"

// TODO tem que fazer toods frontends conectarem ao mesmo primario. como?

__thread char username[MAXNAME]; // TODO thread ou global?
__thread int sock; // TODO inutil?
char primary_host[20];
int primary_port = 0;
REPLICATION_SERVER_t replication_servers[MAXSERVERS];

int primary_sock;
SSL *ssl, *ssl_cls, *primary_ssl, *primary_ssl_sync;
const SSL_METHOD *method_ssl;
SSL_CTX *ctx;

int main(int argc, char * argv[]) {
    struct sockaddr_in address;
    int server_fd, new_socket, port, addrlen, i;
    struct hostent *he;
    struct in_addr **addr_list;

    // REVIEW possibilidade de ter menos de 3? Possibilidade de rodar mais de um server na mesma maquina? (Sem vm). se botar só um servidor, sem porta, dá pau
    // Check number of parameters
    if (argc < 2) {
        printf("Usage: %s <front end port> <primary server ip> <primary server port> <backup server 1 ip> <backup server 1 port>  <backup server 2 ip> <backup server 2 port>\n", argv[0]);
        return 1;
    }

    // Set replication servers
    for (i = 0; i < MAXSERVERS; i++) {
        if (3+2*i <= argc){
            he = gethostbyname( argv[2+2*i] );
            addr_list = (struct in_addr **) he->h_addr_list;
            strcpy(replication_servers[i].ip, inet_ntoa(*addr_list[0]));
            replication_servers[i].port = atoi(argv[3+2*i]);
            replication_servers[i].isAvailable = 1;
            replication_servers[i].socket = NULL;
            /*
            int j;
            for(j = 0; addr_list[j] != NULL; j++) {
                printf("IP para %d: %s", i , inet_ntoa(*addr_list[j]) );
                strcpy(replication_servers[i].ip, inet_ntoa(*addr_list[j]));
            }*/
        } else {
            strcpy(replication_servers[i].ip, "empty");
            replication_servers[i].port = 0;
            replication_servers[i].isAvailable = 0;
            replication_servers[i].socket = NULL;
        }
    }

    // Initialize SSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method_ssl = TLSv1_2_server_method();
    ctx = SSL_CTX_new(method_ssl);
    if (ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

    // Load SSL certificates
    SSL_CTX_use_certificate_file(ctx, "certificates/FrontEndCertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "certificates/FrontEndKeyFile.pem", SSL_FILETYPE_PEM);

    port = atoi(argv[1]);
    printf("FrontEnd started on port %d\n", port);

    // Set values global used for identifying primary server
    strcpy(primary_host, replication_servers[0].ip);
    primary_port = replication_servers[0].port;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Attach socket to the port
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept and incoming connection
    puts("Waiting for incoming connection...");
    addrlen = sizeof(struct sockaddr_in);
    new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen);
    if(new_socket){
        // Attach SSL
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, new_socket);

        /* do SSL-protocol accept */
        if ( SSL_accept(ssl) == -1 ){
            ERR_print_errors_fp(stderr);
        }

        puts("Connection accepted");

        client_server_handler();

        puts("Handler assigned");
    } else if (new_socket < 0) {
        perror("accept failed");
        return 1;
    }

    printf("Client disconnected. Exiting frontend...\n" );

    SSL_CTX_free(ctx); // release context

    return 0;
}

// TODO melhorar msgs de logs e simplificar esse frontend

// Handle sync connection from server to client
void *server_sync_handler() {
    int read_size;
    char buffer[MSGSIZE];
    debug_printf("ssh: esperando pra ler\n");
    // Keep listening to client requests
    while ((read_size = SSL_read(primary_ssl_sync, buffer, MSGSIZE)) > 0 ) {
        // end of string marker
        buffer[read_size] = '\0';
        printf("RECEBI %d bytes: %s\n", read_size, buffer);

        SSL_write(ssl_cls, buffer, MSGSIZE);
        printf("enviei pro cliente cls\n");
    }
}

// Handle server response
void *server_response_handler() {
    int read_size;
    char buffer[MSGSIZE];
    debug_printf("srh: esperando pra ler\n");
    // Keep listening to client requests
    while ((read_size = SSL_read(primary_ssl, buffer, MSGSIZE)) > 0 ) {
        // end of string marker
        buffer[read_size] = '\0';
        printf("RECEBI %d bytes: %s\n", read_size, buffer);

        SSL_write(ssl, buffer, MSGSIZE);
        printf("enviei pro cliente\n");
    }
}

// Handle connection from client to server
void *client_server_handler() {
    int addrlen_cls, read_size, valread, i;
    char client_ip[20], buffer[MSGSIZE], serverinfo[150], port[4];
    struct sockaddr_in addr;
    int server_fd_cls, new_socket_cls;
    struct sockaddr_in address_cls;
    uint16_t primary_sync_port;

    // Receive username
    read_size = SSL_read(ssl, username, sizeof(username));
    username[read_size] = '\0';

    // Send "OK" to confirm connection was accepted.
    SSL_write(ssl, TRANSMISSION_CONFIRM, MSGSIZE);

    // Create socket for client's 'server'
    // Creating socket file descriptor
    if ((server_fd_cls = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address_cls.sin_family = AF_INET;
    address_cls.sin_addr.s_addr = INADDR_ANY;
    address_cls.sin_port = 0; // auto assign port

    // Attach socket to the port
    if (bind(server_fd_cls, (struct sockaddr *) &address_cls, sizeof(address_cls)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd_cls, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    socklen_t len_cls = sizeof(address_cls);
    if (getsockname(server_fd_cls, (struct sockaddr *)&address_cls, &len_cls) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }

    // Send port to the client
    sprintf(buffer, "%d", ntohs(address_cls.sin_port)); // TODO integer? long?
    SSL_write(ssl, buffer, MSGSIZE);

    // Accept an incoming connection
    addrlen_cls = sizeof(struct sockaddr_in);
    new_socket_cls = accept(server_fd_cls, (struct sockaddr *) &address_cls, (socklen_t *) &addrlen_cls);
    if (new_socket_cls < 0) {
        perror("accept failed");
        return 1;
    }

    // Attach SSL
    ssl_cls = SSL_new(ctx);
    SSL_set_fd(ssl_cls, new_socket_cls);

    /* do SSL-protocol accept */
    if ( SSL_accept(ssl_cls) == -1 ){
        ERR_print_errors_fp(stderr);
    }

    // Connect to server
    primary_ssl = connect_server(primary_host, primary_port);
    if (primary_ssl == NULL) {
        return NULL;
    }

    // Tell server this connection is from a frontend
    sprintf(buffer, "%s", CONNECTION_FRONTEND);
    SSL_write(primary_ssl, buffer, MSGSIZE);

    // Check if it's server first connection
    SSL_read(primary_ssl, buffer, MSGSIZE);
    // If it is, send the list of servers
    if (strncmp(buffer, CONNECTION_FIRST, CONNECTION_MSG_SIZE) == 0){
        buffer[0] = '\0';
        serverinfo[0] = '\0';

        for(i = 0; i < MAXSERVERS; i++){
            if(replication_servers[i].isAvailable){
                sprintf(serverinfo, "%s|%d|", replication_servers[i].ip, replication_servers[i].port);
                strcat(buffer, serverinfo);
            }
        }
        SSL_write(primary_ssl, buffer, MSGSIZE);
    }


    // Send username to server
    SSL_write(primary_ssl, username, sizeof(username));

    // Detect if connection was closed // TODO funcionando?? manter aqui e no cliente??
    valread = SSL_read(primary_ssl, buffer, MSGSIZE);
    debug_printf("Buffer: %s\n", buffer);
    if (valread == 0 || strncmp(buffer, TRANSMISSION_CONFIRM, TRANSMISSION_MSG_SIZE) != 0) {
        printf("%s is already connected in two devices. Closing connection...\n", username);
        return 0;
    }

    // Receive server port for new socket
    SSL_read(primary_ssl, buffer, MSGSIZE);
    primary_sync_port = atoi(buffer); // TODO nao usar atoi? atol? outras funcoes mais seguras?
    debug_printf("primary_sync_port: %d\n", primary_sync_port);

    // Connect and attach SSL
    primary_ssl_sync = connect_server(primary_host, primary_sync_port);
    if (primary_ssl_sync == NULL) {
        return NULL;
    }

    // Create thread for listening server sync
    pthread_t thread_id, thread_id2;
    if (pthread_create(&thread_id, NULL, server_response_handler, NULL) < 0) {
        perror("could not create thread");
        return NULL;
    }
    if (pthread_create(&thread_id2, NULL, server_sync_handler, NULL) < 0) {
        perror("could not create thread");
        return NULL;
    }

    // Keep listening to client requests
    while ((read_size = SSL_read(ssl, buffer, MSGSIZE)) > 0 ) {
        // end of string marker
        buffer[read_size] = '\0';
        printf("RECEBI do cliente %d bytes: %s\n", read_size, buffer);

        SSL_write(primary_ssl, buffer, MSGSIZE);
        printf("enviei pro servidor\n");
    }

    return NULL;
}

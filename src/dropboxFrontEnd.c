#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../include/dropboxFrontEnd.h"

// TODO colocar mutex no frontend!!

char primary_host[20];
int primary_port = 0;
REPLICATION_SERVER_t replication_servers[MAXSERVERS];
FRONTEND_CLIENT_t clients[MAXCLIENTS];


int primary_sock;
SSL *ssl, *ssl_cls, *primary_ssl, *primary_ssl_sync; // TODO conferir se ta certo. Não deveria ser exclusivo da thread? pq aí caso tenha mais de um usuario ao mesmo tempo pode acabar confundindo os sockets...?
const SSL_METHOD *method_ssl;
SSL_CTX *ctx;


__thread char username[MAXNAME];
__thread int sock;

// TODO confirmar se conexao com servidores funcionou e informar quantos backups estao sendo usados

int main(int argc, char * argv[]) {
    struct sockaddr_in address;
    int server_fd, new_socket, port, addrlen, i;

    // REVIEW possibilidade de ter menos de 3? Possibilidade de rodar mais de um server na mesma maquina? (Sem vm)
    // Check number of parameters
    if (argc < 2) {
        printf("Usage: %s <front end port> <primary server ip> <primary server port> <backup server 1 ip> <backup server 1 port>  <backup server 2 ip> <backup server 2 port>\n", argv[0]);
        return 1;
    }

    // Set userid to FREE_CLIENT_SLOT_USERID for all the clients, so we can check if a slot is free later
    for (i = 0; i < MAXCLIENTS; i++) {
        strcpy(clients[i].userid, FREE_CLIENT_SLOT_USERID);
        clients[i].devices[0] = NULL;
        clients[i].devices[1] = NULL;
        clients[i].devices_server[0] = NULL;
        clients[i].devices_server[1] = NULL;
    }

    // Set userid to FREE_CLIENT_SLOT_USERID for all the clients, so we can check if a slot is free later
    for (i = 0; i < MAXSERVERS; i++) {
        strcpy(replication_servers[i].ip, argv[2+2*i]);
        replication_servers[i].port = atoi(argv[3+2*i]);
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
    SSL_CTX_use_certificate_file(ctx, "certificates/CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "certificates/KeyFile.pem", SSL_FILETYPE_PEM);

    port = atoi(argv[1]);
    printf("FrontEnd started on port %d\n", port);

    // TODO tentar conectar em um dos servidores, e caso dÊ tudo certo definir
    // ele como primario. Caso nao consiga conectar com nenhum do servidores, encerra (avisa o cliente?_

    // Set values global used for identifying primary server
    strcpy(primary_host, replication_servers[0].ip);
    primary_port = replication_servers[0].port;

    // TODO conexao inicial tá sem SSL? deveria ter?
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
    puts("Waiting for incoming connections...");
    addrlen = sizeof(struct sockaddr_in);
    pthread_t thread_id;
    while ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen))) {
        // Attach SSL
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, new_socket);

        /* do SSL-protocol accept */
        if ( SSL_accept(ssl) == -1 ){
            ERR_print_errors_fp(stderr);
        }

        puts("Connection accepted");

        if (pthread_create(&thread_id, NULL, client_server_handler, (void *) &new_socket) < 0) {
            perror("could not create thread");
            return 1;
        } // TODO Passar socket com SSL como parametro?
        // Now join the thread, so that we dont terminate before the thread
        // pthread_join(thread_id, NULL);
        puts("Handler assigned");
    }
    if (new_socket < 0) {
        perror("accept failed");
        return 1;
    }

    SSL_CTX_free(ctx); // release context // TODO is it called on graceful_exit? i guess no

    return 0;
}


// Handle sync connection from server to client
void *server_sync_handler(void *socket_desc) { // TODO Passar socket com SSL como parametro?
    // Get the socket descriptor
    sock = *(int *) socket_desc;
    // TODO setar device_to_use d eacordo e user_id
    int device_to_use = 0, user_id = 0;
    int read_size;
    char buffer[MSGSIZE];
    debug_printf("ssh: esperando pra ler\n");
    // Keep listening to client requests
    while ((read_size = SSL_read(primary_ssl_sync, buffer, MSGSIZE)) > 0 ) {
        // end of string marker
        buffer[read_size] = '\0';
        printf("RECEBI do servidor %d bytes: %s\n", read_size, buffer);

        SSL_write(clients[user_id].devices_server[device_to_use], buffer, MSGSIZE);
        printf("enviei pro cliente cls\n");
        // TODO ler quantos caracteres?
        // TODO enviar pro servidor
    }
}

// Handle server response
void *server_response_handler(void *socket_desc) { // TODO Passar socket com SSL como parametro?
    // Get the socket descriptor
    sock = *(int *) socket_desc;
    // TODO setar device_to_use d eacordo e user_id
    int device_to_use = 0, user_id = 0;
    int read_size;
    char buffer[MSGSIZE];
    debug_printf("srh: esperando pra ler\n");
    // Keep listening to client requests
    while ((read_size = SSL_read(primary_ssl, buffer, MSGSIZE)) > 0 ) {
        // end of string marker
        buffer[read_size] = '\0';
        printf("RECEBI %d bytes: %s\n", read_size, buffer);

        SSL_write(clients[user_id].devices[device_to_use], buffer, MSGSIZE);
        printf("enviei pro cliente\n");
        // TODO ler quantos caracteres?
        // TODO enviar pro servidor
    }
}


// Handle connection from client to server
void *client_server_handler(void *socket_desc) {
    // Get the socket descriptor
    sock = *(int *) socket_desc;
    int addrlen_cls, read_size, valread, user_id, uid, device_to_use;
    char client_ip[20], buffer[MSGSIZE];
    struct sockaddr_in addr;
    int server_fd_cls, new_socket_cls;
    struct sockaddr_in address_cls;
    uint16_t port_converted, primary_sync_port;

    // Get socket addr and client IP
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(sock, (struct sockaddr *)&addr, &addr_size);
    strcpy(client_ip, inet_ntoa(addr.sin_addr));

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
    // TODO tratar retorno do accept?
    debug_printf("ACEITOU CONEXAO\n");

    // Attach SSL
    ssl_cls = SSL_new(ctx);
    SSL_set_fd(ssl_cls, new_socket_cls);

    /* do SSL-protocol accept */
    if ( SSL_accept(ssl_cls) == -1 ){
        ERR_print_errors_fp(stderr);
    }


    // Search for user in list of clients
    for(user_id = 0; user_id < MAXCLIENTS; user_id++){
        if(strcmp(clients[user_id].userid, username) == 0){
          break;
        }
    }

    // If found
    if(user_id != MAXCLIENTS){
        // If it's connected only in one device, connect the second device.
        device_to_use = (clients[user_id].devices[0] < 0) ? 0 : 1;
        clients[user_id].devices[device_to_use] = ssl;
        clients[user_id].devices_server[device_to_use] = ssl_cls;
        debug_printf("device_to_use: %d\n", device_to_use);
    } else {
      // Insert in first free slot
      for(uid = 0; uid < MAXCLIENTS; uid++) {
          if(strcmp(clients[uid].userid, FREE_CLIENT_SLOT_USERID) == 0){
              strcpy(clients[uid].userid, username);
              device_to_use = 0;
              clients[uid].devices[device_to_use] = ssl;
              clients[uid].devices_server[device_to_use] = ssl_cls;
              user_id = uid;
              break;
          }
      }
      // If there's no slot free
      if(uid == MAXCLIENTS){
          return 0; // TODO colocar msg de erro dizendo q passou do limite de usuarios
      }
    }
    // TODO quando cliente é desconectado setar username para 'avaialble'

    // Connect to server
    primary_ssl = connect_server(primary_host, primary_port);
    if (primary_ssl == NULL) {
        return NULL;
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

    // TODO Save SSL socket (for connecting to server) in client structure? para funcioanr com multiplos clientes...

    // Create thread for listening server sync
    pthread_t thread_id, thread_id2;
    if (pthread_create(&thread_id, NULL, server_response_handler, (void *) &primary_ssl) < 0) {
        perror("could not create thread");
        return NULL;
    }
    if (pthread_create(&thread_id2, NULL, server_sync_handler, (void *) &primary_ssl_sync) < 0) {
        perror("could not create thread");
        return NULL;
    }


    // Keep listening to client requests
    while ((read_size = SSL_read(clients[user_id].devices[device_to_use], buffer, MSGSIZE)) > 0 ) {
        // end of string marker
        buffer[read_size] = '\0';
        printf("RECEBI do cliente %d bytes: %s\n", read_size, buffer);

        SSL_write(primary_ssl, buffer, MSGSIZE);
        printf("enviei pro servidor\n");
        // TODO ler quantos caracteres?
        // TODO enviar pro servidor
    }


    return NULL;
}

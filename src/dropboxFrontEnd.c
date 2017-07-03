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

__thread char username[MAXNAME];
__thread int sock;
char primary_host[256];
int primary_port = 0;
int primary_sock;
SSL *ssl, *ssl_cls, *primary_ssl; // TODO conferir se ta certo. Não deveria ser exclusivo da thread? pq aí caso tenha mais de um usuario ao mesmo tempo pode acabar confundindo os sockets...?
const SSL_METHOD *method_server, *method_client;
SSL_CTX *ctx, *ctx_client;

int main(int argc, char * argv[]) {
    struct sockaddr_in address;
    int server_fd, new_socket, port, addrlen;

    // REVIEW possibilidade de ter menos de 3? Possibilidade de rodar mais de um server na mesma maquina? (Sem vm)
    // Check number of parameters
    if (argc < 2) {
        printf("Usage: %s <front end port> <primary server ip> <primary server port> <backup server 1 ip> <backup server 1 port>  <backup server 2 ip> <backup server 2 port>\n", argv[0]);
        return 1;
    }

    strcpy(primary_host, argv[2]);
    primary_port = atoi(argv[3]);

    // Initialize SSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method_server = TLSv1_2_server_method();
    method_client = TLSv1_2_client_method();
    ctx = SSL_CTX_new(method_server);
    ctx_client = SSL_CTX_new(method_client);
    if (ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

    port = atoi(argv[1]);
    printf("FrontEnd started on port %d\n", port);

    // Load SSL certificates
    SSL_CTX_use_certificate_file(ctx, "certificates/CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "certificates/KeyFile.pem", SSL_FILETYPE_PEM);

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
        }
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

// Handle connection from client to server
void *client_server_handler(void *socket_desc) {
    // Get the socket descriptor
    sock = *(int *) socket_desc;
    int addrlen_cls, read_size, valread;
    char client_ip[20], buffer[1024];
    struct sockaddr_in addr;

    // Get socket addr and client IP
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(sock, (struct sockaddr *)&addr, &addr_size);
    strcpy(client_ip, inet_ntoa(addr.sin_addr));

    // Receive username
    read_size = SSL_read(ssl, username, sizeof(username));
    username[read_size] = '\0';

    // Send "OK" to confirm connection was accepted.
    SSL_write(ssl, TRANSMISSION_CONFIRM, TRANSMISSION_MSG_SIZE);

    // Criar socket
    int server_fd_cls, new_socket_cls;
    struct sockaddr_in address_cls;
    uint16_t port_converted;

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
    port_converted = address_cls.sin_port;
    SSL_write(ssl, &port_converted, sizeof(port_converted));

    // Accept and incoming connection
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

    // Connect to server
    primary_sock = connect_server(primary_host, primary_port);
    if (primary_sock < 0) {
        return -1;
    }
    // Attach SSL
    primary_ssl = SSL_new(ctx_client);
    SSL_set_fd(primary_ssl, primary_sock);
    if (SSL_connect(primary_ssl) == -1){
        printf("SSL connection refused\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    debug_printf("vou escrever username\n");

    // Send username to server
    SSL_write(primary_ssl, username, strlen(username));

    debug_printf("escrevi username\n");

    // Detect if connection was closed // TODO funcionando?? manter aqui e no cliente??
    valread = SSL_read(primary_ssl, buffer, TRANSMISSION_MSG_SIZE);
    if (valread == 0 || strcmp(buffer, TRANSMISSION_CONFIRM) != 0) {
        printf("%s is already connected in two devices. Closing connection...\n", username);
        return 0;
    }

    return NULL;
}

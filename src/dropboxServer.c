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
#include "../include/dropboxServer.h"

// TODO as vezes ta aceitando um usuario conectar em mais de 2 dispositivos, e aí fica zoado

// TODO Enviar mensagem dizendo que é o servidor conectando quando conectar com outros servidores

__thread char username[MAXNAME], user_sync_dir_path[256];
__thread int sock; // TODO remover?
__thread CLIENT_t *pClientEntry; // pointer to client struct in list of clients
__thread SSL *ssl;
SSL *ssl_cls; // TODO conferir se ta certo. Não deveria ser exclusivo da thread? pq aí caso tenha mais de um usuario ao mesmo tempo pode acabar confundindo os sockets...?
const SSL_METHOD *method;
SSL_CTX *ctx;

REPLICATION_SERVER_t replication_servers[MAXSERVERS];
int isPrimary = 0;
int isFirstConnection = 1;

pthread_mutex_t clientCreationLock = PTHREAD_MUTEX_INITIALIZER; // prevent concurrent login

TAILQ_HEAD(, tailq_entry) clients_tailq_head; // List of clients

// Exits gracefully, closing all connections
void graceful_exit(int signum) {
    int i;
    struct tailq_entry *client_node;

    printf("\nClosing all connections...\n");
    TAILQ_FOREACH(client_node, &clients_tailq_head, entries) {
        for(i = 0; i < MAXDEVICES; i++ ) {
            if(client_node->client_entry.devices[i] != NULL && client_node->client_entry.devices[i] != ssl) {
                SSL_shutdown(client_node->client_entry.devices[i]);
                SSL_shutdown(client_node->client_entry.devices_server[i]);
            }
        }
    }
    printf("Exiting.\n");

    exit(0);
}

int main(int argc, char * argv[]) {
    struct sockaddr_in address;
    int server_fd, new_socket, port, addrlen;
    struct sigaction action;

    // Check number of parameters
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Initialize SSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = TLSv1_2_server_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

    port = atoi(argv[1]);
    printf("Server started on port %d\nPress Ctrl + C to exit gracefully\n", port);

    // Initialize the tail queue
    TAILQ_INIT(&clients_tailq_head);

    // SIGINT handling
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = graceful_exit;
    sigaction(SIGINT, &action, NULL);

    // Load SSL certificates
    SSL_CTX_use_certificate_file(ctx, "certificates/ServerCertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "certificates/ServerKeyFile.pem", SSL_FILETYPE_PEM);

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
        debug_printf("ccc\n");

        puts("Connection accepted");

        if (pthread_create(&thread_id, NULL, connection_handler, ssl) < 0) {
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

// Send number of files to client and send PUSH for each file
void sync_server() {
    char method[MSGSIZE], buffer[MSGSIZE];
    int i, d;
    uint16_t nList = 0;

    printf("<~ %s requested SYNC\n", username);

    pthread_mutex_lock(&pClientEntry->mutex);

    // Calculate number of files and send to client
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE)
            nList ++;
    }
    sprintf(buffer, "%d", nList);
    SSL_write(ssl, buffer, MSGSIZE);

    // Find current device
    for(d = 0; d < MAXDEVICES; d++ ) {
        if(pClientEntry->devices[d] == ssl) {
            break;
        }
    }

    // Send filenames
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE) {
            printf("[PUSH %s to %s's device %d]\n", pClientEntry->file_info[i].name, username, d);
            sprintf(method, "PUSH %s", pClientEntry->file_info[i].name);
            debug_printf("pClientEntry: %d\n", pClientEntry->devices_server[d]);
            SSL_write(pClientEntry->devices_server[d], method, sizeof(method));
        }
    }

    pthread_mutex_unlock(&pClientEntry->mutex);
}

void receive_file(char *file) {
    int valread, file_i, file_found = -1, first_free_index = -1, i;
    uint32_t nLeft, file_size;
    char buffer[MSGSIZE] = {0}, method[MSGSIZE], file_path[256];
    time_t client_file_time;
    struct utimbuf new_times;

    pthread_mutex_lock(&pClientEntry->mutex);

    // Search for file in user's list of files
    for(file_i = 0; file_i < MAXFILES; file_i++) {
        if(strcmp(pClientEntry->file_info[file_i].name, file) == 0) {
            file_found = file_i;
            break;
        }
    }

    // Find the next free slot in user's list of files
    for(file_i = 0; file_i < MAXFILES; file_i++) {
        if(pClientEntry->file_info[file_i].size == FREE_FILE_SIZE && first_free_index == -1)
            first_free_index = file_i;
    }

    sprintf(file_path, "%s/%s", user_sync_dir_path, file);

    if (isPrimary) {
        // Send server time to client
        SSL_read(ssl, buffer, MSGSIZE); // reads "TIME" request
        send_time();
    }

    // Receive file modification time
    SSL_read(ssl, buffer, MSGSIZE);
    client_file_time = atol(buffer); // TODO nao usar atoi? atol? outras funcoes mais seguras?

    // If file already exists and server's version is newer, it's not transfered.
    if((file_found >= 0) && (client_file_time < pClientEntry->file_info[file_found].last_modified)) {
        printf("Client file is older than server version\n");
        SSL_write(ssl, TRANSMISSION_CANCEL, MSGSIZE);
        pthread_mutex_unlock(&pClientEntry->mutex);
        // Send server file to client
        for(i = 0; i < MAXDEVICES; i++ ) {
            if(pClientEntry->devices[i] == ssl) {
                printf("[PUSH %s to %s's device %d]\n", file, pClientEntry->userid, i);
                sprintf(method, "PUSH %s", file);
                SSL_write(pClientEntry->devices_server[i], method, sizeof(method));
            }
        }
    } else {
        /* Create file where data will be stored */
        FILE *fp, *fp2;
        fp = fopen(file_path, "wb");
        if (NULL == fp) {
            printf("Error opening file");
            SSL_write(ssl, TRANSMISSION_CANCEL, MSGSIZE);
            pthread_mutex_unlock(&pClientEntry->mutex);
        } else {
            // Send "OK" to confirm file was created and is newer than the server version, so it should be transfered
            SSL_write(ssl, TRANSMISSION_CONFIRM, MSGSIZE);

            // Receive file size
            SSL_read(ssl, buffer, MSGSIZE);
            nLeft = atoi(buffer); // TODO nao usar atoi?
            file_size = nLeft;

            debug_printf("Buffer:%s file_size: %d / nLeft = %d / filepath = %s\n", buffer, file_size, nLeft, file_path);

            // Receive data in chunks
            while (nLeft > 0 && (valread = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
                fwrite(buffer, 1, MIN(nLeft,valread), fp);
                nLeft -= MIN(nLeft,valread);
                debug_printf("valread: %d / nLeft = %d\n", valread, nLeft);
            }
            if (valread < 0) {
                printf("\n Read Error \n");
            }

            debug_printf("[Receive file: Closing file]\n");
            fclose (fp);

            new_times.modtime = client_file_time; // set mtime to original file time
            new_times.actime = time(NULL); // set atime to current time
            utime(file_path, &new_times);

            // If file already exists on client file list, update it. Otherwise, insert it.
            if(file_found >= 0) {
                pClientEntry->file_info[file_found].size = file_size;
                pClientEntry->file_info[file_found].last_modified = client_file_time;
            } else {
                strcpy(pClientEntry->file_info[first_free_index].name, file);
                pClientEntry->file_info[first_free_index].size = file_size;
                pClientEntry->file_info[first_free_index].last_modified = client_file_time;
            }

            pthread_mutex_unlock(&pClientEntry->mutex);

            // Send file to other connected devices
            if (isPrimary) {
                for(i = 0; i < MAXDEVICES; i++ ) {
                    if(pClientEntry->devices[i] != NULL && pClientEntry->devices[i] != ssl) {
                        printf("[PUSH %s to %s's device %d]\n", file, pClientEntry->userid, i);
                        sprintf(method, "PUSH %s", file);
                        SSL_write(pClientEntry->devices_server[i], method, sizeof(method));
                    }
                }
                // TODO fazer a replicacao usando threads!! essa funcao aqui ta super duplicada
                int foundFirst = 0; // First server available is the primary one
                for(i = 0; i < MAXSERVERS; i++ ) {
                    if (replication_servers[i].isAvailable == 0) { continue; } // Skip if not available
                    if (foundFirst == 0) { foundFirst = 1; continue; } // Skip if is the primary one

                    printf("[UPLOAD %s to server %s:%d ]\n", file, replication_servers[i].ip, replication_servers[i].port);
                    sprintf(method, "UPLOAD %s", file);
                    SSL_write(replication_servers[i].socket, method, sizeof(method));

                    // Send file logical modification time
                    sprintf(buffer, "%d", client_file_time); // TODO integer? long?
                    SSL_write(replication_servers[i].socket, buffer, MSGSIZE);

                    // Detect if file was created and local version is newer than server version, so file must be transfered
                    valread = SSL_read(replication_servers[i].socket, buffer, MSGSIZE);
                    debug_printf("filepath:%s file:%s\n", file_path, file);
                    fp2 = fopen(file_path, "rb");
                    if (NULL == fp2) {
                        printf("Error opening file");
                        SSL_write(replication_servers[i].socket, TRANSMISSION_CANCEL, MSGSIZE);
                        //pthread_mutex_unlock(&pClientEntry->mutex);
                    }
                    debug_printf("leuuuuu\n");
                    if (strncmp(buffer, TRANSMISSION_CONFIRM, TRANSMISSION_MSG_SIZE) == 0) {
                        debug_printf("ooooo\n");
                        // Send file size to replication server
                        sprintf(buffer, "%d", file_size); // TODO integer? long?
                        debug_printf("filesize: %s", buffer);
                        SSL_write(replication_servers[i].socket, buffer, MSGSIZE);
                        debug_printf("Enviou filesize para replicas");

                        // Read data from file and send it
                        while (1) {
                            // First read file in chunks of 1024 bytes
                            unsigned char buff[MSGSIZE] = {0};
                            int nread = fread(buff, 1, sizeof(buff), fp2);
                            buff[nread] = '\0';

                            // If read was success, send data.
                            if (nread > 0) {
                                SSL_write(replication_servers[i].socket, buff, MSGSIZE);
                            }
                            debug_printf("nread = %d\n", nread);
                            // Either there was error, or reached end of file
                            if (nread < sizeof(buff)) {
                                //if (feof(fp))
                                //    debug_printf("End of file\n");
                                if (ferror(fp2)) {
                                    printf("Error reading\n");
                                }
                                break;
                            }
                        }
                    }
                    fclose(fp2);
                }
            }
        }
    }
};

void send_file(char * file) {
    char file_path[256];
    struct stat st;
    char buffer[MSGSIZE] = {0};

    sprintf(file_path, "%s/%s", user_sync_dir_path, file);

    pthread_mutex_lock(&pClientEntry->mutex);

    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) { // If file exists
        FILE *fp = fopen(file_path,"rb");
        if (fp == NULL) {
            printf("File open error");
            SSL_write(ssl, TRANSMISSION_CANCEL, MSGSIZE);
        } else {
            // Send "OK" exists and was opened
            SSL_write(ssl, TRANSMISSION_CONFIRM, MSGSIZE);

            // Send file size to client
            sprintf(buffer, "%d", st.st_size); // TODO integer? long?
            SSL_write(ssl, buffer, MSGSIZE);

            // Read data from file and send it
            while (1) {
                // First read file in chunks
                unsigned char buff[MSGSIZE] = {0};
                int nread = fread(buff, 1, sizeof(buff), fp);
                buff[nread] = '\0';

                // If read was success, send data.
                if (nread > 0) {
                    SSL_write(ssl, buff, MSGSIZE);
                }

                // Either there was error, or reached end of file
                if (nread < sizeof(buff)) {
                    /*if (feof(fp))
                        debug_printf("End of file\n"); */
                    if (ferror(fp)) {
                        printf("Error reading\n");
                    }
                    break;
                }
            }
        }
        fclose(fp);

        // Send file modification time
        sprintf(buffer, "%ld", st.st_mtime); // TODO integer? long?
        SSL_write(ssl, buffer, MSGSIZE);
    } else {
        printf("File doesn't exist!\n");
        SSL_write(ssl, TRANSMISSION_CANCEL, MSGSIZE);
    }
    pthread_mutex_unlock(&pClientEntry->mutex);
}



void delete_file(char *file) {
    char file_path[256];
    int file_i, found_file = 0, i;
    char method[MSGSIZE];

    sprintf(file_path, "%s/%s", user_sync_dir_path, file);

    pthread_mutex_lock(&pClientEntry->mutex);

    // Search for file in user's list of files
    for(file_i = 0; file_i < MAXFILES; file_i++) {
        // Stop if it is found
        if(strcmp(pClientEntry->file_info[file_i].name, file) == 0) {
            found_file = 1;
            break;
        }
    }

    if(found_file) {
        // Set it's area free
        strcpy(pClientEntry->file_info[file_i].name, "filename.ext");
        pClientEntry->file_info[file_i].last_modified = time(NULL);
        pClientEntry->file_info[file_i].size = FREE_FILE_SIZE;

        // Erase file
        if (remove(file_path) == 0) {
            printf("File deleted\n");
        } else {
            printf("Unable to delete file\n");
        }

        // Delete file from other connected devices
        if (isPrimary) {
            for(i = 0; i < MAXDEVICES; i++ ) {
                if(pClientEntry->devices[i] != NULL && pClientEntry->devices[i] != ssl) {
                    printf("[DELETE %s to %s's device %d]\n", file, pClientEntry->userid, i);
                    sprintf(method, "DELETE %s", file);
                    SSL_write(pClientEntry->devices_server[i], method, sizeof(method));
                }
            }
            // TODO fazer a replicacao usando threads!!
            int foundFirst = 0; // First server available is the primary one
            for(i = 0; i < MAXSERVERS; i++ ) {
                if (replication_servers[i].isAvailable == 0) { continue; } // Skip if not available
                if (foundFirst == 0) { foundFirst = 1; continue; } // Skip if is the primary one

                printf("[DELETE %s to server %s:%d ]\n", file, replication_servers[i].ip, replication_servers[i].port);
                sprintf(method, "DELETE %s", file);
                SSL_write(replication_servers[i].socket, method, sizeof(method));
            }
        }
    } else {
        printf("File not found\n");
    }
    pthread_mutex_unlock(&pClientEntry->mutex);

};

void list_files() {
    char filename_string[MAXNAME];
    int i;
    char buffer[MSGSIZE] = {0};
    uint32_t nList = 0;

    printf("<~ %s requested LIST\n", username);

    pthread_mutex_lock(&pClientEntry->mutex);

    debug_printf("pClientEntry->userid: %s / pClientEntry->devices[0] = %d / pClientEntry->devices[1] = %d / SSL = %d\n", pClientEntry->userid, pClientEntry->devices[0], pClientEntry->devices[1], ssl);
    // Calculate number of files and send to client
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE)
            nList += strlen(pClientEntry->file_info[i].name) + 1;
    }
    sprintf(buffer, "%d", nList); // TODO integer? long?
    SSL_write(ssl, buffer, MSGSIZE);

    // Send filenames
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE) {
            sprintf(filename_string, "%s\n", pClientEntry->file_info[i].name);
            SSL_write(ssl, filename_string, strlen(filename_string));
        }
    }

    pthread_mutex_unlock(&pClientEntry->mutex);
}

void send_time() {
    char buffer[MSGSIZE];
    printf("<~ %s requested TIME\n", username);
    // Send current time
    uint32_t current_time = time(NULL);
    sprintf(buffer, "%ld", current_time);
    SSL_write(ssl, buffer, MSGSIZE);
}

void free_device() {
    struct tailq_entry *client_node;
    struct tailq_entry *tmp_client_node;

    pthread_mutex_lock(&clientCreationLock); // TODO confirmar se resolveu
    // pthread_mutex_lock(&pClientEntry->mutex);

    for (client_node = TAILQ_FIRST(&clients_tailq_head); client_node != NULL; client_node = tmp_client_node) {
        if (strcmp(client_node->client_entry.userid, username) == 0) {
            // Set current sock device free
            if (client_node->client_entry.devices[0] == ssl) {
                client_node->client_entry.devices[0] = NULL;
                client_node->client_entry.devices_server[0] = NULL;
            } else if (client_node->client_entry.devices[1] == ssl) {
                client_node->client_entry.devices[1] = NULL;
                client_node->client_entry.devices_server[1] = NULL;
            }

            // Set logged_in to 0 if user is not connected anymore in any device.
            if (client_node->client_entry.devices[0] == NULL && client_node->client_entry.devices[1] == NULL) {
                client_node->client_entry.logged_in = 0;
            }

            break;
        }
        tmp_client_node = TAILQ_NEXT(client_node, entries);
    }

    pthread_mutex_unlock(&clientCreationLock);
    // pthread_mutex_unlock(&pClientEntry->mutex);
}

void connect_to_replication_servers(){
    // TODO passar pros outros servidores caso seja o primario

    int i, j;
    char buffer[MSGSIZE] = {0}, serverinfo[120];

    int foundFirst = 0; // First server available is the primary one
    for(i = 0; i < MAXSERVERS; i++){
        if (replication_servers[i].isAvailable == 0) { continue; } // Skip if not available
        if (foundFirst == 0) { foundFirst = 1; continue; } // Skip if is the primary one

        replication_servers[i].socket = connect_server(replication_servers[i].ip, replication_servers[i].port);
        // TODO criar threads para evitar que caso nao consiga conectar em um servidor, possa conectar nos outros
        debug_printf("conectou no servidor i=%d socket=%d\n", i, replication_servers[i].socket);
        // Tell server this connection is from a primary server
        sprintf(buffer, "%s", CONNECTION_SERVER);
        SSL_write(replication_servers[i].socket, buffer, MSGSIZE);
        debug_printf("fasofksaofm\n");
        // Check if it's server first connection
        SSL_read(replication_servers[i].socket, buffer, MSGSIZE);
        // If it is, send the list of servers
        if (strncmp(buffer, CONNECTION_FIRST, CONNECTION_MSG_SIZE) == 0){
            buffer[0] = '\0';
            serverinfo[0] = '\0';

            debug_printf("msg de first no %d\n", i);

            for(j = 0; j < MAXSERVERS; j++){
                sprintf(serverinfo, "%s|%d|%d|", replication_servers[i].ip, replication_servers[i].port, replication_servers[i].isAvailable);
                strcat(buffer, serverinfo);
            }
            SSL_write(replication_servers[i].socket, buffer, MSGSIZE);
        }

        // Send username to server
        SSL_write(replication_servers[i].socket, username, sizeof(username));
    }
}

// Handle connection for each client
void *connection_handler(void *socket_desc) {
    // Get the socket descriptor
    ssl = socket_desc;
    int read_size, i, n, device_to_use, addrlen_cls;
    int *nullReturn = NULL;
    char file_path[256], client_ip[20], client_message[MSGSIZE], buffer[MSGSIZE];
    struct sockaddr_in addr;
    struct tailq_entry *client_node, *tmp_client_node;
    struct dirent **namelist;
    struct stat st;
    char *token;

    debug_printf("SSL: %d\n", ssl);

    // Send message to front end informing if it is the first connection
    if (isFirstConnection){

        // Read if connnection is from a frontend or a server
        SSL_read(ssl, buffer, MSGSIZE);
        if (strncmp(buffer, CONNECTION_FRONTEND, CONNECTION_FIRST_MSG_SIZE) == 0) {
            isPrimary = 1;
        }

        debug_printf("Connection from a %s\n", buffer);

        sprintf(buffer, "%s", CONNECTION_FIRST);
        SSL_write(ssl, buffer, MSGSIZE);
        isFirstConnection = 0;

        // Read list of servers
        SSL_read(ssl, buffer, MSGSIZE);
        // Get first server
        token = strtok(buffer, "|");

        i = 0;
        while (token != NULL) {
            strcpy(replication_servers[i].ip, token);
            // Get port
            token = strtok(NULL, "|");
            replication_servers[i].port = atoi(token);
            // Get available
            token = strtok(NULL, "|");
            replication_servers[i].isAvailable = atoi(token);
            // Get next server
            token = strtok(NULL, "|");
            i++;
        }
    } else{
        sprintf(buffer, CONNECTION_NOT_FIRST);
        SSL_write(ssl, buffer, MSGSIZE);
    }

    debug_printf("server vai receber username\n");

    // Receive username
    read_size = SSL_read(ssl, username, sizeof(username));
    username[read_size] = '\0';

    debug_printf("USERNAME: %s\n", username);

    // Define path to user folder on server
    sprintf(user_sync_dir_path, "%s/server_sync_dir_%s", getenv("HOME"), username);

    // Create folder if it doesn't exist
    makedir_if_not_exists(user_sync_dir_path);

    pthread_mutex_lock(&clientCreationLock);

    // Search for client in client list
    for (client_node = TAILQ_FIRST(&clients_tailq_head); client_node != NULL; client_node = tmp_client_node) {
        if (strcmp(client_node->client_entry.userid, username) == 0) {
            break;
        }
        tmp_client_node = TAILQ_NEXT(client_node, entries);
    }

    // If it's found...
    if (client_node != NULL) {
        // and is already connected in two devices, return an error message and close connection.
        if (client_node->client_entry.devices[0] > 0 && client_node->client_entry.devices[1] > 0) {
            printf("Client already connected in two devices. Closing connection...\n");
            pthread_mutex_unlock(&clientCreationLock);
            SSL_shutdown(ssl);
            pthread_exit(nullReturn);

        }
        // If it's connected only in one device, connect the second device.
        device_to_use = (client_node->client_entry.devices[0] < 0) ? 0 : 1;
        client_node->client_entry.devices[device_to_use] = ssl;
    }
    // If it's not found, it's not connected, so it need to be added to the client list.
    else {
        client_node = malloc(sizeof(*client_node));
        if (client_node == NULL) {
            perror("malloc failed");
            pthread_mutex_unlock(&clientCreationLock);
            pthread_exit(nullReturn);
        }

        device_to_use = 0;
        client_node->client_entry.devices[0] = ssl;
        client_node->client_entry.devices[1] = NULL;
        strcpy(client_node->client_entry.userid, username);
        client_node->client_entry.logged_in = 1;
        client_node->client_entry.mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        // Set size to FREE_FILE_SIZE for all the files, so we can check if a slot is free later
        for (i = 0; i < MAXFILES; i++) {
            client_node->client_entry.file_info[i].size = FREE_FILE_SIZE;
        }

        // Insert data into struct file_info
        n = scandir(user_sync_dir_path, &namelist, 0, alphasort);
        if (n > 2) { // Starting in i=2, it doesn't show '.' and '..'
            for (i = 2; i < n; i++) {
                sprintf(file_path, "%s/%s", user_sync_dir_path, namelist[i]->d_name);
                stat(file_path, &st);

                strcpy(client_node->client_entry.file_info[i-2].name, namelist[i]->d_name);
                client_node->client_entry.file_info[i-2].last_modified = st.st_mtime;
                client_node->client_entry.file_info[i-2].size = st.st_size;

                free(namelist[i]);
            }
        }
        free(namelist);

        TAILQ_INSERT_TAIL(&clients_tailq_head, client_node, entries);
    }
    pClientEntry = &(client_node->client_entry);

    pthread_mutex_unlock(&clientCreationLock);

    // Send "OK" to confirm connection was accepted.
    SSL_write(ssl, TRANSMISSION_CONFIRM, MSGSIZE);

    if (isPrimary){
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
        sprintf(buffer, "%d", ntohs(address_cls.sin_port)); // TODO integer? long?
        SSL_write(ssl, buffer, MSGSIZE);

        debug_printf("Socket CLS: %d\n", ntohs(port_converted));

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

        // Connect to client's server
        client_node->client_entry.devices_server[device_to_use] = ssl_cls;

        connect_to_replication_servers();
    }

    // Receive requests from client
    while ((read_size = SSL_read(ssl, client_message, MSGSIZE)) > 0 ) {
        // end of string marker
        client_message[read_size] = '\0';

        if (!strncmp(client_message, "LIST", 4)) {
            list_files();
            printf("~> List of files sent to %s on device %d\n", username, device_to_use);
        } else if (!strncmp(client_message, "DOWNLOAD", 8)) {
            printf("Request method: DOWNLOAD\n");
            send_file(client_message + 9);
        } else if (!strncmp(client_message, "UPLOAD", 6)) {
            printf("Request method: UPLOAD\n");
            receive_file(client_message + 7);
        } else if (!strncmp(client_message, "DELETE", 6)) {
            printf("Request method: DELETE\n");
            delete_file(client_message + 7);
        } else if (!strncmp(client_message, "SYNC", 4)) {
            printf("Request method: SYNC\n");
            sync_server();
        } else if (!strncmp(client_message, "TIME", 4)) {
            printf("Request method: TIME\n");
            send_time();
        }
    }

    if (read_size == 0) {
        printf("<~ %s disconnected\n", username);
        free_device();
        fflush(stdout);
    }	else if(read_size == -1) {
        perror("recv failed");
    }

    return NULL;
}

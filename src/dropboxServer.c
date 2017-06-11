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
#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

__thread char username[MAXNAME], user_sync_dir_path[256];
__thread int sock;
__thread CLIENT_t *pClientEntry; // pointer to client struct in list of clients

pthread_mutex_t clientCreationLock = PTHREAD_MUTEX_INITIALIZER; // prevent concurrent login

TAILQ_HEAD(, tailq_entry) clients_tailq_head; // List of clients

// Exits gracefully, closing all connections and without interrupting file transfers. TODO add mutex? prevent interrupting file transfers
void graceful_exit(int signum) {
    int i;
    char method[] = "CLOSE";
    struct tailq_entry *client_node;

    printf("\nClosing all connections...\n");
    TAILQ_FOREACH(client_node, &clients_tailq_head, entries) {
        for(i = 0; i < MAXDEVICES; i++ ) {
            if(client_node->client_entry.devices[i] != INVALIDSOCKET && client_node->client_entry.devices[i] != sock) {
                write(client_node->client_entry.devices_server[i], method, sizeof(method));
                printf("~> Sent CLOSE to %s's device %d\n", client_node->client_entry.userid, i);
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

    port = atoi(argv[1]);
    printf("Server started on port %d\nPress Ctrl + C to exit gracefully\n", port);

    // Initialize the tail queue
    TAILQ_INIT(&clients_tailq_head);

    // SIGINT handling
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = graceful_exit;
    sigaction(SIGINT, &action, NULL);

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
        puts("Connection accepted");

        if (pthread_create(&thread_id, NULL, connection_handler, (void *) &new_socket) < 0) {
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

    return 0;
}

// Send number of files to client and send PUSH for each file
void sync_server() {
    char method[METHODSIZE];
    int i, d;
    uint16_t nList = 0, nListConverted;

    printf("<~ %s requested SYNC\n", username);

    pthread_mutex_lock(&pClientEntry->mutex);

    // Calculate number of files and send to client
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE)
            nList ++;
    }
    nListConverted = htons(nList);
    write(sock, &nListConverted, sizeof(nListConverted));

    // Find current device
    for(d = 0; d < MAXDEVICES; d++ ) {
        if(pClientEntry->devices[d] == sock) {
            break;
        }
    }

    // Send filenames
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE) {
            printf("[PUSH %s to %s's device %d]\n", pClientEntry->file_info[i].name, username, d);
            sprintf(method, "PUSH %s", pClientEntry->file_info[i].name);
            write(pClientEntry->devices_server[d], method, sizeof(method));
        }
    }

    pthread_mutex_unlock(&pClientEntry->mutex);
}

void receive_file(char *file) {
    int valread, file_i, file_found = -1, first_free_index = -1, i;
    uint32_t nLeft, file_size;
    char buffer[1024] = {0}, method[METHODSIZE], file_path[256];
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

    // Receive file modification time
    valread = read(sock, &client_file_time, sizeof(client_file_time));

    // If file already exists and server's version is newer, it's not transfered.
    if((file_found >= 0) && (client_file_time < pClientEntry->file_info[file_found].last_modified)) {
        printf("Client file is older than server version\n");
        write(sock, "ERROR", 5);
        pthread_mutex_unlock(&pClientEntry->mutex);
        // Send server file to client
        for(i = 0; i < MAXDEVICES; i++ ) {
            if(pClientEntry->devices[i] == sock) {
                printf("[PUSH %s to %s's device %d]\n", file, pClientEntry->userid, i);
                sprintf(method, "PUSH %s", file);
                write(pClientEntry->devices_server[i], method, sizeof(method));
            }
        }
    } else {
        /* Create file where data will be stored */
        FILE *fp;
        fp = fopen(file_path, "wb");
        if (NULL == fp) {
            printf("Error opening file");
            write(sock, "ERROR", 5);
            pthread_mutex_unlock(&pClientEntry->mutex);
        } else {
            // Send "OK" to confirm file was created and is newer than the server version, so it should be transfered
            write(sock, "OK", 2);

            // Receive file size
            valread = read(sock, &file_size, sizeof(file_size));
            nLeft = ntohl(file_size);

            // Receive data in chunks
            while (nLeft > 0 && (valread = read(sock, buffer, (MIN(sizeof(buffer), nLeft)))) > 0) {
                fwrite(buffer, 1, valread, fp);
                nLeft -= valread;
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
            for(i = 0; i < MAXDEVICES; i++ ) {
                if(pClientEntry->devices[i] != INVALIDSOCKET && pClientEntry->devices[i] != sock) {
                    printf("[PUSH %s to %s's device %d]\n", file, pClientEntry->userid, i);
                    sprintf(method, "PUSH %s", file);
                    write(pClientEntry->devices_server[i], method, sizeof(method));
                }
            }
        }
    }
};

void send_file(char * file) {
    char file_path[256];
    struct stat st;
    uint32_t length_converted;

    sprintf(file_path, "%s/%s", user_sync_dir_path, file);

    pthread_mutex_lock(&pClientEntry->mutex);

    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) { // If file exists
        FILE *fp = fopen(file_path,"rb");
        if (fp == NULL) {
            printf("File open error");
            write(sock, "ERROR", 5);
        } else {
            // Send "OK" exists and was opened
            write(sock, "OK", 2);

            // Send file size to client
            length_converted = htonl(st.st_size);
            write(sock, &length_converted, sizeof(length_converted));

            // Read data from file and send it
            while (1) {
                // First read file in chunks of 1024 bytes
                unsigned char buff[1024] = {0};
                int nread = fread(buff, 1, sizeof(buff), fp);

                // If read was success, send data.
                if (nread > 0) {
                    write(sock, buff, nread);
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
        write(sock, &st.st_mtime, sizeof(st.st_mtime)); // TODO use htonl and ntohl?
    } else {
        printf("File doesn't exist!\n");
        write(sock, "ERROR", 5);
    }
    pthread_mutex_unlock(&pClientEntry->mutex);
}

void delete_file(char *file) {
    char file_path[256];
    int file_i, found_file = 0, i;
    char method[METHODSIZE];

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
        for(i = 0; i < MAXDEVICES; i++ ) {
            if(pClientEntry->devices[i] != INVALIDSOCKET && pClientEntry->devices[i] != sock) {
                printf("[DELETE %s to %s's device %d]\n", file, pClientEntry->userid, i);
                sprintf(method, "DELETE %s", file);
                write(pClientEntry->devices_server[i], method, sizeof(method));
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
    uint32_t nList = 0, nListConverted;

    printf("<~ %s requested LIST\n", username);

    pthread_mutex_lock(&pClientEntry->mutex);

    // Calculate number of files and send to client
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE)
            nList += strlen(pClientEntry->file_info[i].name) + 1;
    }
    nListConverted = htonl(nList);
    write(sock, &nListConverted, sizeof(nListConverted));

    // Send filenames
    for (i = 0; i < MAXFILES; i++) {
        if (pClientEntry->file_info[i].size != FREE_FILE_SIZE) {
            sprintf(filename_string, "%s\n", pClientEntry->file_info[i].name);
            write(sock, filename_string, strlen(filename_string));
        }
    }

    pthread_mutex_unlock(&pClientEntry->mutex);
}

void free_device() {
    struct tailq_entry *client_node;
    struct tailq_entry *tmp_client_node;

    pthread_mutex_lock(&pClientEntry->mutex);

    for (client_node = TAILQ_FIRST(&clients_tailq_head); client_node != NULL; client_node = tmp_client_node) {
        if (strcmp(client_node->client_entry.userid, username) == 0) {
            // Set current sock device free
            if (client_node->client_entry.devices[0] == sock) {
                client_node->client_entry.devices[0] = INVALIDSOCKET;
                client_node->client_entry.devices_server[0] = INVALIDSOCKET;
            } else if (client_node->client_entry.devices[1] == sock) {
                client_node->client_entry.devices[1] = INVALIDSOCKET;
                client_node->client_entry.devices_server[1] = INVALIDSOCKET;
            }

            // Set logged_in to 0 if user is not connected anymore in any device.
            if (client_node->client_entry.devices[0] == INVALIDSOCKET && client_node->client_entry.devices[0] == INVALIDSOCKET) {
                client_node->client_entry.logged_in = 0;
            }

            break;
        }
        tmp_client_node = TAILQ_NEXT(client_node, entries);
    }

    pthread_mutex_unlock(&pClientEntry->mutex);
}

// Handle connection for each client
void *connection_handler(void *socket_desc) {
    // Get the socket descriptor
    sock = *(int *) socket_desc;
    int read_size, i, n, device_to_use;
    uint16_t client_server_port;
    int *nullReturn = NULL;
    char file_path[256], client_ip[20], client_message[METHODSIZE];
    struct sockaddr_in addr;
    struct tailq_entry *client_node, *tmp_client_node;
    struct dirent **namelist;
    struct stat st;

    // Get socket addr and client IP
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(sock, (struct sockaddr *)&addr, &addr_size);
    strcpy(client_ip, inet_ntoa(addr.sin_addr));

    // Receive username
    read_size = recv(sock, username, sizeof(username), 0);
    username[read_size] = '\0';

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
            shutdown(sock, 2);
            pthread_exit(nullReturn);

        }
        // If it's connected only in one device, connect the second device.
        device_to_use = (client_node->client_entry.devices[0] < 0) ? 0 : 1;
        client_node->client_entry.devices[device_to_use] = sock;
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
        client_node->client_entry.devices[0] = sock;
        client_node->client_entry.devices[1] = INVALIDSOCKET;
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
    write(sock, "OK", 2);

    // Receive client's local server port
    read_size = recv(sock, &client_server_port, sizeof(client_server_port), 0);
    client_server_port = ntohs(client_server_port);
    debug_printf("Client's local server port: %d\n", client_server_port);

    // Connect to client's server
    client_node->client_entry.devices_server[device_to_use] = connect_server(client_ip, client_server_port);

    // Receive requests from client
    while ((read_size = recv(sock, client_message, METHODSIZE, 0)) > 0 ) {
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

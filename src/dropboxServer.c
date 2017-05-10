#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

int main(int argc, char * argv[]) {
    if (argc != 2) { // Número incorreto de parametros
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

	int port = atoi(argv[1]);
    printf("Server started on port %d\n", port);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    /*if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, & opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    } TODO remove this? it's not working on mac and seems unnecessary */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr * ) & address,
            sizeof(address)) < 0) {
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
    while((new_socket = accept(server_fd, (struct sockaddr * ) & address,
            (socklen_t * ) & addrlen))) {
        puts("Connection accepted");
        if(pthread_create( &thread_id, NULL,  connection_handler, (void*) &new_socket) < 0) {
            perror("could not create thread");
            return 1;
        }
        // Now join the thread, so that we dont terminate before the thread
        // pthread_join( thread_id, NULL);
        puts("Handler assigned");
    }
    if (new_socket < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}

void sync_server() {

}

void receive_file(char * file) {

}

void send_file(char * file) {

}

// Handle connection for each client
void *connection_handler(void *socket_desc) {
	// Get the socket descriptor
	int sock = *(int*)socket_desc;
	int read_size;
	char client_message[1024];

	// Receive a message from client
	while( (read_size = recv(sock, client_message, 1024, 0)) > 0 )  {
		// end of string marker
		client_message[read_size] = '\0';

        // LIST method
        if (!strncmp(client_message, "LIST", 4)) {
            char user_sync_dir_path[256];
            char username[] = "usuario"; // TODO get user from where?
            struct dirent **namelist;
            int i, n;

            printf("<~ %s requested LIST\n", username);

            // Define path to user folder on server
            sprintf(user_sync_dir_path, "%s/server_sync_dir_%s", getenv("HOME"), username);

            // Create folder if it doesn't exist
            // TODO it should be done at the first time a user connects to the server, not here.
            makedir_if_not_exists(user_sync_dir_path);

            // List files
            n = scandir(user_sync_dir_path, &namelist, 0, alphasort);
            if(n >= 0){
                for (i = 2; i < n; i++) { // Starting in i=2, it doesn't show '.' and '..'
                    // TODO whats the difference between write and send methods?
                    write(sock, namelist[i]->d_name, strlen(namelist[i]->d_name));
                    write(sock, "\n", 1);
                    free(namelist[i]);
                }
            } else {
                perror("Couldn't open the directory");
            }
            free(namelist);

    		printf("~> List of files sent to %s\n", username);

        } else if (!strncmp(client_message, "DOWNLOAD", 8)) {

            printf("Request method: DOWNLOAD\n");
            printf("Filename: %s\n", client_message + 9);

        } else if (!strncmp(client_message, "UPLOAD", 6)) {

            printf("Request method: UPLOAD\n");
    		printf("Filename: %s\n", client_message + 7);

        };
	}
	if(read_size == 0) {
		puts("Client disconnected");
		fflush(stdout);
	}
	else if(read_size == -1) {
		perror("recv failed");
	}

	return 0;
}

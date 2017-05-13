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

char username[MAXNAME];

int main(int argc, char * argv[]) {
    int server_fd, new_socket, port;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Check number of parameters
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

	  port = atoi(argv[1]);
    printf("Server started on port %d\n", port);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr * ) & address,
            sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) { // TODO checar se esse valor 3 é a melhor opção
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
  char send_buffer[1024];
  char user_sync_dir_path[256];
  char filename_string[256];

  read_size = recv(sock, username, sizeof(username), 0);

  // Define path to user folder on server
  sprintf(user_sync_dir_path, "%s/server_sync_dir_%s", getenv("HOME"), username);

  // Create folder if it doesn't exist
  makedir_if_not_exists(user_sync_dir_path);

	// Receive a message from client
	while( (read_size = recv(sock, client_message, sizeof(client_message), 0)) > 0 )  {
		// end of string marker
		client_message[read_size] = '\0';


        // LIST method
        if (!strncmp(client_message, "LIST", 4)) {
            struct dirent **namelist;
            int i, n;

            printf("<~ %s requested LIST\n", username);

            // List files
            n = scandir(user_sync_dir_path, &namelist, 0, alphasort);
            if(n > 2){
                for (i = 2; i < n; i++) { // Starting in i=2, it doesn't show '.' and '..'
                    // TODO should we use send or sendto? need to check whats the difference between write and them...
                    sprintf(filename_string, "%s\n", namelist[i]->d_name);
                    // if buffer has enough space, insert complete filename
                    if(strlen(send_buffer) + strlen(filename_string) < sizeof(send_buffer)){
                      strcpy(send_buffer+strlen(send_buffer), filename_string);
                      free(namelist[i]);
                    } else {
                      int space_available = sizeof(send_buffer) - strlen(send_buffer) -1; // TODO -1 ?
                      strncpy(send_buffer+strlen(send_buffer), filename_string, space_available); // insere no buffer o numero de caracteres que ainda cabem
                      write(sock, send_buffer, strlen(send_buffer));
                      strcpy(send_buffer+strlen(send_buffer), filename_string+space_available);
                      free(namelist[i]);
                    }
                }
                write(sock, send_buffer, strlen(send_buffer));
                write(sock, "", 1);
            } else if (n >= 0) { // empty directory
                write(sock, "", 1);
            } else {
                write(sock, "", 1);
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
		printf("<~ %s disconnected\n", username);
		fflush(stdout);
	}
	else if(read_size == -1) {
		perror("recv failed");
	}

	return 0;
}

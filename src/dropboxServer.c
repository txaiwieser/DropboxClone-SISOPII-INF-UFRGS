#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

int main(int argc, char * argv[]) {
    char cmd[256];
    char * token;
		int port;

    if (argc != 2) { // Número incorreto de parametros
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

		port = atoi(argv[1]);
    printf("Server started on port %d\n", port);

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, & opt, sizeof(opt))) {
        perror("setsockopt");
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
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    if ((new_socket = accept(server_fd, (struct sockaddr * ) & address,
            (socklen_t * ) & addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    valread = read(new_socket, buffer, 1024);

    if (!strncmp(buffer, "LIST", 4)) {
        printf("Request method: LIST\n");
				char * list_of_files = "filename1.ext\nfilename2.ext\nfilename3.ext\n";
				send(new_socket, list_of_files, strlen(list_of_files), 0);
				printf("List of files sent\n");
    } else if (!strncmp(buffer, "DOWNLOAD", 8)) {
        printf("Request method: DOWNLOAD\n");
        printf("Filename: %s\n", buffer + 9);
    } else if (!strncmp(buffer, "UPLOAD", 6)) {
        printf("Request method: UPLOAD\n");
				printf("Filename: %s\n", buffer + 7);
    };
		// TODO o que fazer se for algum método invalido?

    printf("BUFFER: %s\n", buffer);

    return 0;
}

void sync_server() {

}

void receive_file(char * file) {

}

void send_file(char * file) {

}

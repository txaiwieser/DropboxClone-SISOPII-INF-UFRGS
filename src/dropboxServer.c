#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

#define PORT 3003

int main(){
								printf("Server started\n");

								int server_fd, new_socket, valread;
								struct sockaddr_in address;
								int opt = 1;
								int addrlen = sizeof(address);
								char buffer[1024] = {0};
								char *list_of_files = "filename1.ext\nfilename2.ext\nfilename3.ext\n";

// Creating socket file descriptor
								if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
								{
																perror("socket failed");
																exit(EXIT_FAILURE);
								}

// Forcefully attaching socket to the port 8080
								if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
																							&opt, sizeof(opt)))
								{
																perror("setsockopt");
																exit(EXIT_FAILURE);
								}
								address.sin_family = AF_INET;
								address.sin_addr.s_addr = INADDR_ANY;
								address.sin_port = htons( PORT );

// Forcefully attaching socket to the port 8080
								if (bind(server_fd, (struct sockaddr *)&address,
																	sizeof(address))<0)
								{
																perror("bind failed");
																exit(EXIT_FAILURE);
								}
								if (listen(server_fd, 3) < 0)
								{
																perror("listen");
																exit(EXIT_FAILURE);
								}
								if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
																																	(socklen_t*)&addrlen))<0)
								{
																perror("accept");
																exit(EXIT_FAILURE);
								}
								valread = read( new_socket, buffer, 1024);

								if( !strncmp(buffer, "LIST", 4) ) {
																printf("Request method: LIST\n");
								}
								else if( !strncmp(buffer, "GET_SYNC_DIR", 12) ) {
																printf("Request method: GET_SYNC_DIR\n");
								}
								else if( !strncmp(buffer, "DOWNLOAD", 8) ) {
																printf("Request method: DOWNLOAD\n");
																printf("Filename: %s\n", buffer+8);
								}
								else if( !strncmp(buffer, "UPLOAD", 6) ) {
																printf("Request method: UPLOAD\n");
																// TODO get filename
								};

								printf("BUFFER: %s\n",buffer );
								send(new_socket, list_of_files, strlen(list_of_files), 0 );
								printf("List of files sent\n");


								return 0;
}

void sync_server(){

}

void receive_file(char *file){

}

void send_file(char *file){

}

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
#include <unistd.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

__thread char username[MAXNAME];
__thread char user_sync_dir_path[256];
__thread int sock;
__thread CLIENT_t *pClientEntry; // pointer to client struct in client list

TAILQ_HEAD(, tailq_entry) my_tailq_head;

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

    // Initialize the tail queue
    TAILQ_INIT(&my_tailq_head);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) { // REVIEW Is 3 the best value for backlog (2nd parameter)?
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

void sync_server() {
  // TODO sync_server()
}

void receive_file(char *file) {
  int valread;
  int32_t nLeft, file_size;
  char buffer[1024] = {0};
  char file_path[256];
  time_t original_file_time;
  struct utimbuf new_times;
  int file_i, found_file = 0, first_free_index = -1;

  // Search for file in user struct.
  for(file_i = 0; file_i < MAXFILES && !found_file; file_i++){
    // Stop if file is found
    if(strcmp(pClientEntry->file_info[file_i].name, file) == 0)
      found_file = 1;
    // Find the next free index
    if(pClientEntry->file_info[file_i].size == FREE_FILE_SIZE && first_free_index == -1)
      first_free_index = file_i;
  }

  sprintf(file_path, "%s/%s", user_sync_dir_path, file);

  // Receive original filetime
  valread = read(sock, &original_file_time, sizeof(original_file_time));

  // If file already exists and server's version is newer, it's not transfered.
  if(found_file && (original_file_time < pClientEntry->file_info[file_i].last_modified)){
    printf("Client file is older than server version\n");
  }
  //FIXME logica tá errada.. vai enviar OK semrpe.. e se nao enviar ok, oq vai acontecer com client?

  /* Create file where data will be stored */
  FILE *fp;
  fp = fopen(file_path, "wb");
  if (NULL == fp) {
      printf("Error opening file");
  } else {
    // Send "OK" to confirm file was created and is newer than the server version, so it should be transfered
    write(sock, "OK", 2); // TODO resolver de outro jeito? Se definisse um tamanho fixo para todas as mensagens, seria só ajustar o write, como fizemos na outra funcao (que usa MIN(nLeft, sizeof(...)))

    // Receive length
    valread = read(sock, &file_size, sizeof(file_size));
    nLeft = ntohl(file_size);

    /* Receive data in chunks */
    while (nLeft > 0 && (valread = read(sock, buffer, (MIN(sizeof(buffer), nLeft)))) > 0) {
      fwrite(buffer, 1, valread, fp);
      nLeft -= valread;
    }
    if (valread < 0) {
        printf("\n Read Error \n");
    }
  }
  debug_printf("Fechando arquivo\n");
  fclose (fp);

  new_times.modtime = original_file_time; /* set mtime to original file time */
  new_times.actime = time(NULL); /* set atime to current time */
  utime(file_path, &new_times);

  // If file already exists on client file list
  if(found_file){
    // Update
    pClientEntry->file_info[file_i].size = file_size;
    pClientEntry->file_info[file_i].last_modified = original_file_time;
  } else {
    // If it doesn't, insert it
    strcpy(pClientEntry->file_info[first_free_index].name, file);
    pClientEntry->file_info[first_free_index].size = file_size;
    pClientEntry->file_info[first_free_index].last_modified = original_file_time;
  }

  // TODO enviar arquivo para o outro dispositivo do usuario caso esteja conectado
};

void send_file(char * file) {
  char file_path[256];
  int stat_result;
  struct stat st;
  int32_t length_converted;

  sprintf(file_path, "%s/%s", user_sync_dir_path, file);
  stat_result = stat(file_path, &st);

  if (stat_result == 0) { // If file exists
    /* Open the file that we wish to transfer */
    FILE *fp = fopen(file_path,"rb");
    if (fp == NULL) {
        length_converted = htonl(0);
        write(sock, &length_converted, sizeof(length_converted));
        printf("File open error");
    } else {
      /* Send file size to client */
      length_converted = htonl(st.st_size);
      write(sock, &length_converted, sizeof(length_converted));

      /* Read data from file and send it */
      while (1) {
          /* First read file in chunks of 1024 bytes */
          unsigned char buff[1024] = {0};
          int nread = fread(buff, 1, sizeof(buff), fp);

          /* If read was success, send data. */
          if (nread > 0) {
              write(sock, buff, nread);
          }

          /* Either there was error, or reached end of file */
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

      // Send file modtime
      // TODO use htonl and ntohl?
      write(sock, &st.st_mtime, sizeof(st.st_mtime));
    } else {
      printf("File doesn't exist!\n");
  }
}

void list_files(){
  char filename_string[MAXNAME];
  int i;
  int32_t nList = 0, nListConverted;

  printf("<~ %s requested LIST\n", username);

  // List files
  for (i = 0; i < MAXFILES; i++){
    if (pClientEntry->file_info[i].size != FREE_FILE_SIZE)
      nList += strlen(pClientEntry->file_info[i].name) + 1;
  }

  // Send length
  nListConverted = htonl(nList);
  write(sock, &nListConverted, sizeof(nListConverted));

  // Send filenames
  for (i = 0; i < MAXFILES; i++){
    if (pClientEntry->file_info[i].size >= 0){
      sprintf(filename_string, "%s\n", pClientEntry->file_info[i].name);
      write(sock, filename_string, strlen(filename_string));
    }
  }
}

void free_device(){
  struct tailq_entry *client_node;
  struct tailq_entry *tmp_client_node;

  for (client_node = TAILQ_FIRST(&my_tailq_head); client_node != NULL; client_node = tmp_client_node){
    if (strcmp(client_node->client_entry.userid, username) == 0) {
      // Set current sock device free
      if (client_node->client_entry.devices[0] == sock) {
        client_node->client_entry.devices[0] = -1;
      } else if (client_node->client_entry.devices[1] == sock) {
        client_node->client_entry.devices[1] = -1;
      }

      // Set logged_in to 0 if user is not connected anymore in any device.
      if (client_node->client_entry.devices[0] == -1 && client_node->client_entry.devices[0] == -1){
        client_node->client_entry.logged_in = 0;
      }

      break;
    }
    tmp_client_node = TAILQ_NEXT(client_node, entries);
  }
}

// Handle connection for each client
void *connection_handler(void *socket_desc) {
	// Get the socket descriptor
	sock = *(int *) socket_desc;
	int read_size;
	char client_message[METHODSIZE];
  struct tailq_entry *client_node;
  struct tailq_entry *tmp_client_node;
  int *nullReturn = NULL;
  struct dirent **namelist;
  int i, n;
  struct stat st;
  char file_path[256];

  // Receive username from client
  read_size = recv(sock, username, sizeof(username), 0);
  username[read_size] = '\0';

  // Define path to user folder on server
  sprintf(user_sync_dir_path, "%s/server_sync_dir_%s", getenv("HOME"), username);

  // Create folder if it doesn't exist
  makedir_if_not_exists(user_sync_dir_path);

  // Search for client in client list
  for (client_node = TAILQ_FIRST(&my_tailq_head); client_node != NULL; client_node = tmp_client_node) {
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
      shutdown(sock, 2);
      pthread_exit(nullReturn);
    }
    // If it's connected only in one device, connect the second device.
    int device_to_use = (client_node->client_entry.devices[0] < 0) ? 0 : 1;
    client_node->client_entry.devices[device_to_use] = sock;
  }
  // If it's not found, it's not connected, so it need to be added to the client list.
  else {
    client_node = malloc(sizeof(*client_node));
  	if (client_node == NULL) {
  		perror("malloc failed");
      pthread_exit(nullReturn);
  	}

    client_node->client_entry.devices[0] = sock;
    client_node->client_entry.devices[1] = -1;
    strcpy(client_node->client_entry.userid, username);
    client_node->client_entry.logged_in = 1;

    // Set size to -1 for all the files, so we can check if a slot is free later
    // REVIEW any better way to solve this?
    for (i = 0; i < MAXFILES; i++){
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
    } else {
        perror("Couldn't open the directory or it's empty"); // TODO remove?
    }
    free(namelist);

    TAILQ_INSERT_TAIL(&my_tailq_head, client_node, entries);
  }
  pClientEntry = &(client_node->client_entry);

  // Send "OK" to confirm connection was accepted.
  write(sock, "OK", 2);

	// Receive a message from client
	while ((read_size = recv(sock, client_message, METHODSIZE, 0)) > 0 ) {
		// end of string marker
		client_message[read_size] = '\0';

        if (!strncmp(client_message, "LIST", 4)) {
            list_files();
    		    printf("~> List of files sent to %s\n", username);
        } else if (!strncmp(client_message, "DOWNLOAD", 8)) {
            printf("Request method: DOWNLOAD\n");
            send_file(client_message + 9);
        } else if (!strncmp(client_message, "UPLOAD", 6)) {
            printf("Request method: UPLOAD\n");
            receive_file(client_message + 7);
        };
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

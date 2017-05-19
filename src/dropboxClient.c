#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <libgen.h>
#include <pthread.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

// REVIEW Check if strings sizes and types used (mostly int) are enough
// REVIEW do these variables need to be global?
char server_host[256];
int server_port = 0, sock = 0;
char server_user[MAXNAME];
char user_sync_dir_path[256];
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

// TODO Handle errors on send_file, get_file, receive_file (on server), send_file (on server). If file can't be opened, it should return an error and exit. Also, display success messages.
void send_file(char *file) {
    int stat_result;
    char method[160];
    struct stat st;
    int32_t length_converted;

    stat_result = stat(file, &st);

    if (stat_result == 0) { // If file exists
      /* Open the file that we wish to transfer */
      FILE *fp = fopen(file,"rb");
      if (fp == NULL) {
        printf("Couldn't open the file\n");
      } else {
          // Concatenate strings to get method = "UPLOAD filename"
          sprintf(method, "UPLOAD %s", basename(file));

          // Call to the server
          send(sock, method, strlen(method), 0);
          debug_printf("[%s method sent]\n", method);
          sleep(1); // FIXME Remover. Coloquei apenas para testar
          /* Send file size to server */
          length_converted = htonl(st.st_size);
          write(sock, &length_converted, sizeof(length_converted));
          sleep(1); // FIXME Remover. Coloquei apenas para testar
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
    } else {
      printf("File doesn't exist! Pass a valid filename.\n");
    }
}

// TODO confirmar se tá funcionando pra downloads seguidos de arquivos grandes e pequenos ou se tá com o mesmo problema que tava a funcao de upload
void get_file(char *file) {
    int valread;
    int32_t nLeft;
    char buffer[1024] = {0};
    char method[160];
    char file_path[256];

    sprintf(file_path, "%s/%s", user_sync_dir_path, file);

    // Concatenate strings to get method = "DOWNLOAD filename"
    sprintf(method, "DOWNLOAD %s", file);

    // Send to the server
    send(sock, method, strlen(method), 0);
    debug_printf("[%s method sent]\n", method);

    // TODO If file doesn't exist on server, return an error message.

    /* Create file where data will be stored */
    FILE *fp;
    fp = fopen(file_path, "wb");
    if (NULL == fp) {
        printf("Error opening file");
    } else {
      // Receive length
      valread = read(sock, &nLeft, sizeof(nLeft));
      nLeft = ntohl(nLeft);

      /* Receive data in chunks */
      while (nLeft > 0 && (valread = read(sock, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, valread, fp);
        nLeft -= valread;
      }
      if (valread < 0) {
          printf("\n Read Error \n");
      }
    }
    fclose (fp);
};

void cmdList() {
    int valread;
    int32_t nLeft;
    char buffer[1024] = {0};

    send(sock, "LIST", 4, 0);

    // Receive length
    read(sock, &nLeft, sizeof(nLeft));
    nLeft = ntohl(nLeft);

    /* Receive data in chunks */
    while (nLeft > 0 && (valread = read(sock, buffer, sizeof(buffer))) > 0) {
      buffer[valread] = '\0';
      printf("%s", buffer);
      nLeft -= valread;
    }
};

void cmdGetSyncDir() {
    // Create user sync_dir if it doesn't exist
    makedir_if_not_exists(user_sync_dir_path);
};

void cmdMan() {
    printf("\nAvailable commands:\n");
    printf("get_sync_dir\n");
    printf("list\n");
    printf("download <filename.ext>\n");
    printf("upload <path/filename.ext>\n");
    printf("help\n");
    printf("exit\n\n");
};

void cmdExit() {
    close_connection();
}

void sync_client(){
  // TODO sync_client
}

void* sync_daemon(void* unused) {
  // TODO exclude hidden files (.swp files segfault?)
  // TODO sleep for 10 seconds?
  // TODO nao pode enviar um arquivo que foi recém baixado pelo comando de download
  int length;
  int fd;
  int wd;
  char buffer[BUF_LEN];

  fd = inotify_init();

  if ( fd < 0 ) {
    perror( "inotify_init" );
  }

  wd = inotify_add_watch( fd, user_sync_dir_path,
                         IN_MODIFY | IN_CREATE | IN_DELETE );

  while (1) {
    int i = 0;
    char filepath[MAXNAME];

    length = read( fd, buffer, BUF_LEN );

    if ( length < 0 ) {
      perror( "read" );
    }

    while ( i < length ) {
      struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
      if ( event->len ) {
          sleep(1); // FIXME esse sleep foi adicionado porque senão o inotify às vezes vê que o arquivo foi criado e acha que ele está pronto para ser enviado ao servdior, mas na verdade ele ainda não foi (completamente) preenchido. Qual o melhor jeito de resolver isso? semaforo?
        if ( event->mask & IN_CREATE ) {
          if ( event->mask & IN_ISDIR ) {
            printf( "The directory %s was created.\n", event->name );
          }
          else {
            printf( "The file %s was created.\n", event->name );
            sprintf(filepath, "%s/%s", user_sync_dir_path, event->name);
            send_file(filepath);
          }
        }
        else if ( event->mask & IN_DELETE ) {
          if ( event->mask & IN_ISDIR ) {
            printf( "The directory %s was deleted.\n", event->name );
          }
          else {
            printf( "The file %s was deleted.\n", event->name );
          }
        }
        else if ( event->mask & IN_MODIFY ) {
          if ( event->mask & IN_ISDIR ) {
            printf( "The directory %s was modified.\n", event->name );
          }
          else {
            printf( "The file %s was modified.\n", event->name );
            sprintf(filepath, "%s/%s", user_sync_dir_path, event->name);
            send_file(filepath);
          }
        }
      }
      i += EVENT_SIZE + event->len;
    }
  }
  ( void ) inotify_rm_watch( fd, wd );
  ( void ) close( fd );

  exit( 0 );

  //TODO verificar se essa thread está fechando quando a main é encerrada
}

int main(int argc, char * argv[]) {
    char cmd[256];
    char filename[256];
    char buffer[1024];
    char * token;
    int valread;
    pthread_t thread_id;

    // Check number of parameteres
    if (argc < 4) {
        printf("Usage: %s <user> <IP> <port>\n", argv[0]);
        return 1;
    }

    // Connect to server
    debug_printf("[Client started with parameters User=%s IP=%s Port=%s]\n", argv[1], argv[2], argv[3]);
    strcpy(server_host, argv[2]);
    strcpy(server_user, argv[1]);
    server_port = atoi(argv[3]);
    sock = connect_server(server_host, server_port);
    if (sock < 0) {
        return -1;
    }

    // Define path to user sync_dir folder
    sprintf(user_sync_dir_path, "%s/sync_dir_%s", getenv("HOME"), server_user);

    // Send username to server
    write(sock, server_user, strlen(server_user));

    // Detect if connection was closed
    valread = read(sock, buffer, sizeof(buffer));
    if (valread == 0) {
      printf("%s already connected in two devices. Closing connection...\n", server_user);
      return 0;
    }

    debug_printf("[Connection established]\n");

    if (pthread_create(&thread_id, NULL, sync_daemon, NULL) < 0) {
        perror("could not create sync_client thread");
        return 1;
    }

    printf("Welcome to Dropbox! - v 1.0\n");
    cmdMan();

    // Handle user input
    while (1) {
        printf("Dropbox> ");
        // TODO Ajustar se usuário tecla enter sem inserir nada antes
        scanf("%s", cmd);
        if ((token = strtok(cmd, " \t")) != NULL) {
            if (strcmp(token, "exit") == 0) break;
            else if (strcmp(token, "upload") == 0) {
                scanf("%s", filename);
                // TODO Copy file to local sync_dir. (What to do if there's already a file with the same name?)
                send_file(filename);
            }
            else if (strcmp(token, "download") == 0) {
                scanf("%s", filename);
                if ((dir_exists(user_sync_dir_path) == 0)) {
                  get_file(filename);
                } else {
                  printf("Run get_sync_dir first.\n");
                }
            }
            else if (strcmp(token, "list") == 0) cmdList();
            else if (strcmp(token, "get_sync_dir") == 0) cmdGetSyncDir();
            else if (strcmp(token, "help") == 0) cmdMan();
            else printf("Invalid command! Type 'help' to see the available commands\n");
        }
    }
    return 0;
}

int connect_server(char * host, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    server = gethostbyname(host);
    if (server == NULL) {
        printf("ERROR, no such host\n");
        return -1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = *((struct in_addr *) server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed. \n");
        return -1;
    }

    return sock;
}

void close_connection() {
    // Stop both reception and transmission.
    shutdown(sock, 2);
}

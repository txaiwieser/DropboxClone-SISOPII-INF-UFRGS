#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#define __USE_XOPEN
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <netdb.h>
#include <libgen.h>
#include <pthread.h>
#include <math.h>
#include <dirent.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

// REVIEW do these variables need to be global?
char server_host[256];
int server_port = 0, sock = 0;
char server_user[MAXNAME];
char user_sync_dir_path[256];
char *pIgnoredFileEntry; // pointer to ignored file in ignored files list

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

TAILQ_HEAD(, tailq_entry) my_tailq_head;

// TODO Handle errors on send_file, get_file, receive_file (on server), send_file (on server). If file can't be opened, it should return an error and exit. Also, display success messages.
void send_file(char *file) {
    int stat_result;
    char method[METHODSIZE];
    struct stat st;
    char buffer[1024] = {0};
    int valread;
    int32_t length_converted;

    stat_result = stat(file, &st);

    if (stat_result == 0 && S_ISREG(st.st_mode)) { // If file exists
        /* Open the file that we wish to transfer */
        FILE *fp = fopen(file,"rb");
        if (fp == NULL) {
            printf("Couldn't open the file\n");
        } else {
            // Concatenate strings to get method = "UPLOAD filename"
            sprintf(method, "UPLOAD %s", basename(file));

            // Call to the server
            write(sock, method, sizeof(method)); // REVIEW Usando sizeof funciona, mas usando strlen dá bug a partir do segundo arquvio. tá certo? (atencao, se alterar, nao esquecer de mudar nas outras funcoes q fazem exatamente a mesma coisa)
            debug_printf("[%s method sent]\n", method);

            // Send file modtime
            write(sock, &st.st_mtime, sizeof(st.st_mtime)); // TODO use htonl and ntohl?

            // Detect if file was created and local version is newer than server version, so file must be transfered
            valread = read(sock, buffer, sizeof(buffer));
            if (valread > 0) {
                /* Send file size to server */
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
        }
        fclose(fp);
    } else {
        printf("File doesn't exist! Pass a valid filename.\n");
    }
}

void get_file(char *file, char *path) {
    int valread;
    int32_t nLeft;
    char buffer[1024] = {0};
    char method[METHODSIZE];
    char file_path[256];
    time_t original_file_time;
    struct utimbuf new_times;
    struct tailq_entry *ignoredfile_node;

    // If current directory is user_sync_dir, overwrite the file. It's another directory, get file only if there's no file with same filename
    if(!file_exists(file) || strcmp(user_sync_dir_path, path) == 0) {

        // Set saving path to current directory
        sprintf(file_path, "%s/%s", path, file);

        // Concatenate strings to get method = "DOWNLOAD filename"
        sprintf(method, "DOWNLOAD %s", file);

        // Send to the server
        write(sock, method, sizeof(method));
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
            while (nLeft > 0 && (valread = read(sock, buffer, (MIN(sizeof(buffer), nLeft)))) > 0) {
                fwrite(buffer, 1, valread, fp);
                nLeft -= valread;
            }
            if (valread < 0) {
                printf("\n Read Error \n");
            }
        }
        fclose (fp);

        // Set modtime to original file modtime
        valread = read(sock, &original_file_time, sizeof(time_t));
        new_times.modtime = original_file_time; /* set mtime to original file time */
        new_times.actime = time(NULL); /* set atime to current time */
        utime(file_path, &new_times);

        // If path is user_sync_dir, insert filename in the list of ignored files
        if(strcmp(user_sync_dir_path, path) == 0){
            ignoredfile_node = malloc(sizeof(*ignoredfile_node));
            strcpy(ignoredfile_node->filename, file);
            if (ignoredfile_node == NULL) {
                perror("malloc failed");
            }
            TAILQ_INSERT_TAIL(&my_tailq_head, ignoredfile_node, entries);
            debug_printf("[Inseriu arquivo %s na lista pois foi baixado]\n", file);
        }
    } else {
        printf("There's already a file named %s in this directory\n", file);
    }
};

void delete_server_file(char *file) {
    char method[METHODSIZE];

    // Concatenate strings to get method = "DOWNLOAD filename"
    sprintf(method, "DELETE %s", file);

    // Send to the server
    write(sock, method, sizeof(method));
    debug_printf("[%s method sent]\n", method);
};

void delete_local_file(char *file) {
    char filepath[MAXNAME];
    struct tailq_entry *ignoredfile_node;
    sprintf(filepath, "%s/%s", user_sync_dir_path, file);
    if(remove(filepath) == 0) {
        // REVIEW Success.  print a message? return a value?

        // Add to ignore list, so inotify doesn't send a DELETE method again to server
        ignoredfile_node = malloc(sizeof(*ignoredfile_node));
        strcpy(ignoredfile_node->filename, file);
        if (ignoredfile_node == NULL) {
            perror("malloc failed");
        }
        TAILQ_INSERT_TAIL(&my_tailq_head, ignoredfile_node, entries);
        debug_printf("[Inseriu arquivo %s na lista pois foi apagado primeiramente em outro dispositivo]\n", file);
    };
};

// Save list of files into user_sync_dir_path/.dropboxfiles
void save_list_of_files(){
  // TODO autosave every x seconds? or maybe everytime there's a file change?
  struct dirent **namelist;
  int n, i;
  FILE *fp;
  struct stat st;
  char filepath[MAXNAME], stfpath[MAXNAME];
  sprintf(filepath, "%s/.dropboxfiles", user_sync_dir_path);

  fp = fopen(filepath, "w");
  if (NULL == fp) {
      printf("Error opening file");
  } else {
      n = scandir(user_sync_dir_path, &namelist, 0, alphasort);

      if (n > 2) { // Starting in i=2, it ignores '.' and '..'
          for (i = 2; i < n; i++) {
              sprintf(stfpath, "%s/%s", user_sync_dir_path, namelist[i]->d_name);
              stat(stfpath, &st);
              if(namelist[i]->d_name[0] != '.' && !(st.st_mode & S_IFDIR)) { // If it's a file and it's not hidden
                  // Save filename
                  fwrite(namelist[i]->d_name, strlen(namelist[i]->d_name), 1, fp);
                  fwrite("\n", 1, 1, fp);
                  // and timestamp
                  struct tm *ptm = gmtime(&st.st_mtime);
                  char buf[256];
                  strftime(buf, sizeof buf, "%F %T", ptm);
                  fwrite(buf, strlen(buf), 1, fp);
                  fwrite("\n", 1, 1, fp);

                  free(namelist[i]);
              }
          }
      }
  }
  debug_printf("[Fechando .dropboxfiles]\n");
  fclose (fp);
}

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
    printf("delete <filename.ext>\t\tDelete file from server\n");
    printf("help\n");
    printf("exit\n\n");
};

void cmdExit() {
    save_list_of_files();
    close_connection();
}

void sync_client() {
    FILE *fp;
    char filepath[MAXNAME], filename[MAXNAME], buf[256];
    sprintf(filepath, "%s/.dropboxfiles", user_sync_dir_path);

    debug_printf("[Syncing...]\n");

    // Open .dropboxfiles
    fp = fopen(filepath, "r");
    if (NULL == fp) {
        // TODO checar se arquivo existe?
        printf("Error opening file");
    } else {
        // Read filenames with timestamps
        while(fgets(filename, sizeof(filename), fp)){
            // Remove \n at end of string
            size_t ln = strlen(filename)-1;
            if (filename[ln] == '\n')
                filename[ln] = '\0';

            fgets(buf, sizeof(buf), fp);
            // Converting from string to time_t
            struct tm tm;
            strptime(buf, "%F %T", &tm);
            time_t t = mktime(&tm);

            debug_printf("File %s modified %s", filename, buf);
        }
        // TODO compare with server and then sync.
    }
    fclose(fp);
    debug_printf("[Syncing done]\n");
}

void* sync_daemon(void* unused) {
    // TODO Não enviar arquivo se ele foi recém baixado. Por exemplo, quando o servidor manda um PUSH pro cliente, este baixa o arquivo e aí o inotify acha que o arquivo foi criado, entao tenta enviá-lo ao servidor, que por sua vez envia push de novo.. ? Testar melhor. Talvez está faltando comparar timestamp na hora de receber um arquivo no servidor?
    int length;
    int fd;
    int wd;
    char buffer[BUF_LEN];
    struct tailq_entry *ignoredfile_node;
    struct tailq_entry *tmp_ignoredfile_node;

    fd = inotify_init(); // Blocking by default

    if ( fd < 0 ) {
        perror( "inotify_init" );
    }

    wd = inotify_add_watch( fd, user_sync_dir_path,
                            IN_CLOSE_WRITE | IN_MOVE | IN_DELETE );

    // TODO testar renomeação

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
                if ( !(event->mask & IN_ISDIR) && event->name[0] != '.' ) { // If it's a file and it's not hidden
                    // Search for file in list of ignored files
                    for (ignoredfile_node = TAILQ_FIRST(&my_tailq_head); ignoredfile_node != NULL; ignoredfile_node = tmp_ignoredfile_node) {
                        if (strcmp(ignoredfile_node->filename, event->name) == 0) {
                            debug_printf("Daemon: Achou arquivo %s na lista\n", event->name);
                            break;
                        }
                        tmp_ignoredfile_node = TAILQ_NEXT(ignoredfile_node, entries);
                    }
                    // If it's not in the list, it was modified locally, so changes need to be propagated to server
                    if(ignoredfile_node == NULL){
                        if ( (event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO)  ) {
                            debug_printf( "The file %s was created, modified, or moved from somewhere.\n", event->name );
                            sprintf(filepath, "%s/%s", user_sync_dir_path, event->name);
                            send_file(filepath);
                        } else if ( event->mask & IN_DELETE  ) {
                            debug_printf( "The file %s was deleted.\n", event->name );
                            delete_server_file(event->name);
                        } else if ( event->mask & IN_MOVED_FROM  ) {
                            debug_printf( "The file %s was renamed.\n", event->name );
                            sprintf(filepath, "%s/%s", user_sync_dir_path, event->name);
                            delete_server_file(event->name);
                        }
                    } else {
                        // Remove file from list
                        TAILQ_REMOVE(&my_tailq_head, ignoredfile_node, entries);
                        /* Free the item as we don’t need it anymore. */
                        free(ignoredfile_node);
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
    ( void ) inotify_rm_watch( fd, wd );
    ( void ) close( fd );

    exit( 0 );
}

// This thread receives 'push file' requests from server
void* local_server(void* unused) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    uint16_t port_converted;
    int read_size;
    char server_message[METHODSIZE];

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0; // auto assign port // REVIEW check bind return to confirm it succeded https://stackoverflow.com/questions/10294515/how-do-i-find-in-c-that-a-port-is-free-to-use

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(address);
    if (getsockname(server_fd, (struct sockaddr *)&address, &len) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }

    // Send local_server port to the server
    port_converted = address.sin_port;
    write(sock, &port_converted, sizeof(port_converted));

    // Accept and incoming connection
    addrlen = sizeof(struct sockaddr_in);
    while ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen))) {
        //puts("Connection accepted. Handler assigned");

        // Receive a message from server
        while ((read_size = recv(new_socket, server_message, METHODSIZE, 0)) > 0 ) {
            // end of string marker
            server_message[read_size] = '\0';

            // TODO PULL method? (maybe another name...)
            if (!strncmp(server_message, "PUSH", 4)) {
                printf("received PUSH from server\n");
                get_file(server_message + 5, user_sync_dir_path);
            } else if (!strncmp(server_message, "DELETE", 6)) {
                printf("received DELETE from server\n");
                delete_local_file(server_message + 7);
            }
        }

    }
    if (new_socket < 0) {
        perror("accept failed");
    }

    exit(0);
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

    // Initialize the tail queue
    TAILQ_INIT(&my_tailq_head);

    // Define path to user sync_dir folder
    sprintf(user_sync_dir_path, "%s/sync_dir_%s", getenv("HOME"), server_user);

    // Create user sync_dir if it not exists
    cmdGetSyncDir();

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
        perror("could not create inotify thread");
        return 1;
    }

    if (pthread_create(&thread_id, NULL, local_server, NULL) < 0) {
        perror("could not create local_server thread");
        return 1;
    }

    // Sync client
    sync_client();
    debug_printf("[Device synced]\n");

    printf("Welcome to Dropbox! - v 1.0\n");
    cmdMan();

    // Handle user input
    while (1) {
        printf("Dropbox> ");
        // TODO Ajustar se usuário tecla enter sem inserir nada antes
        scanf("%s", cmd);
        if ((token = strtok(cmd, " \t")) != NULL) {
            if (strcmp(token, "exit") == 0) {
                cmdExit();
                break;
            } else if (strcmp(token, "upload") == 0) {
                scanf("%s", filename);
                // TODO Copy file to local sync_dir. (What to do if there's already a file with the same name?)
                send_file(filename);
            } else if (strcmp(token, "download") == 0) {
                scanf("%s", filename);
                char cwd[256];
                getcwd(cwd, sizeof(cwd));
                get_file(filename, cwd);
            } else if (strcmp(token, "delete") == 0) {
                scanf("%s", filename);
                delete_server_file(filename);
            } else if (strcmp(token, "list") == 0) cmdList();
            else if (strcmp(token, "get_sync_dir") == 0) cmdGetSyncDir();
            else if (strcmp(token, "help") == 0) cmdMan();
            else printf("Invalid command! Type 'help' to see the available commands\n");
        }
    }
    return 0;
}

void close_connection() {
    // Stop both reception and transmission.
    shutdown(sock, 2);
}

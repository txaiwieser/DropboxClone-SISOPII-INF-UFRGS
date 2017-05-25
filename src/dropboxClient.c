#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <netdb.h>
#include <libgen.h>
#include <pthread.h>
#include <math.h>
#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

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

    if(!file_exists(file)) {

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
    if(remove(file) == 0) {
        // REVIEW Success.  print a message? return a value?
    };
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
    printf("delete <filename.ext>\t\tDelete file from server\n");
    printf("help\n");
    printf("exit\n\n");
};

void cmdExit() {
    close_connection();
}

void sync_client() {
    // TODO sync_client
}

void* sync_daemon(void* unused) {
    // TODO Não enviar arquivo se ele foi recém baixado. Por exemplo, quando o servidor manda um PUSH pro cliente, este baixa o arquivo e aí o inotify acha que o arquivo foi criado, entao tenta enviá-lo ao servidor, que por sua vez envia push de novo.. ? Testar melhor. Talvez está faltando comparar timestamp na hora de receber um arquivo no servidor?
    int length;
    int fd;
    int wd;
    char buffer[BUF_LEN];

    fd = inotify_init();

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
                }
            }
            i += EVENT_SIZE + event->len;
        }
        sleep(3); // REVIEW adjust sleep time
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
    if (listen(server_fd, 3) < 0) { // REVIEW Is 3 the best value for backlog (2nd parameter)?
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

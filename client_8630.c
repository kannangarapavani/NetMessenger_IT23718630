#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 14630
#define BUFFER_SIZE 1024
#define MAX_FILENAME 256
#define MAX_USERNAME 32

volatile int connected = 1;


/* =========================
   SEND ALL BYTES
   ========================= */

int send_all(int sockfd,
             const char *buffer,
             size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length) {

        ssize_t sent =
            send(sockfd,
                 buffer + total_sent,
                 length - total_sent,
                 0);

        if (sent <= 0) {
            return -1;
        }

        total_sent += (size_t)sent;
    }

    return 0;
}


/* =========================
   READ ONE TEXT LINE
   ========================= */

ssize_t read_line(int sockfd,
                  char *buffer,
                  size_t max_length)
{
    size_t index = 0;

    while (index < max_length - 1) {

        char ch;

        ssize_t received =
            recv(sockfd,
                 &ch,
                 1,
                 0);

        if (received == 0) {
            return 0;
        }

        if (received < 0) {
            return -1;
        }

        if (ch == '\n') {
            break;
        }

        if (ch != '\r') {
            buffer[index++] = ch;
        }
    }

    buffer[index] = '\0';

    return (ssize_t)index;
}


/* =========================
   RECEIVE EXACT RAW BYTES
   ========================= */

int receive_exact(int sockfd,
                  unsigned char *buffer,
                  size_t length)
{
    size_t total_received = 0;

    while (total_received < length) {

        ssize_t received =
            recv(sockfd,
                 buffer + total_received,
                 length - total_received,
                 0);

        if (received <= 0) {
            return -1;
        }

        total_received += (size_t)received;
    }

    return 0;
}


/* =========================
   SAVE RECEIVED FILE
   ========================= */

int save_received_file(int sockfd,
                       const char *sender,
                       const char *filename,
                       size_t file_size)
{
    char sender_directory[512];
    char path[768];

    if (mkdir("./received", 0755) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    snprintf(sender_directory,
             sizeof(sender_directory),
             "./received/%s",
             sender);

    if (mkdir(sender_directory, 0755) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    snprintf(path,
             sizeof(path),
             "%s/%s",
             sender_directory,
             filename);

    FILE *file =
        fopen(path, "wb");

    if (file == NULL) {
        return -1;
    }

    unsigned char chunk[4096];
    size_t remaining = file_size;

    while (remaining > 0) {

        size_t wanted;

        if (remaining < sizeof(chunk)) {
            wanted = remaining;
        } else {
            wanted = sizeof(chunk);
        }

        ssize_t received =
            recv(sockfd,
                 chunk,
                 wanted,
                 0);

        if (received <= 0) {

            fclose(file);

            return -1;
        }

        size_t written =
            fwrite(chunk,
                   1,
                   (size_t)received,
                   file);

        if (written !=
            (size_t)received) {

            fclose(file);

            return -1;
        }

        remaining -=
            (size_t)received;
    }

    fclose(file);

    printf("File saved to %s\n",
           path);

    return 0;
}


/* =========================
   RECEIVER THREAD
   ========================= */

void *receive_messages(void *arg)
{
    int sockfd =
        *((int *)arg);

    char response[BUFFER_SIZE];

    while (connected) {

        ssize_t received =
            read_line(sockfd,
                      response,
                      sizeof(response));

        if (received == 0) {

            printf("\nServer closed the connection.\n");

            connected = 0;

            break;
        }

        if (received < 0) {

            connected = 0;

            break;
        }


        /*
         * Incoming file header:
         *
         * MSG FILE <sender> <filename> <filesize>
         */
        if (strncmp(response,
                    "MSG FILE ",
                    9) == 0) {

            char sender[MAX_USERNAME];
            char filename[MAX_FILENAME];

            unsigned long long
                file_size_value;


            int fields =
                sscanf(
                    response,
                    "MSG FILE %31s %255s %llu",
                    sender,
                    filename,
                    &file_size_value);


            if (fields == 3) {

                printf(
                    "\nIncoming file: %s from %s (%llu bytes)\n",
                    filename,
                    sender,
                    file_size_value);


                if (save_received_file(
                        sockfd,
                        sender,
                        filename,
                        (size_t)file_size_value) < 0) {

                    printf(
                        "Failed to save received file.\n");
                }


                printf("> ");

                fflush(stdout);

                continue;
            }
        }


        /*
         * Normal text message / response.
         */
        printf("\n%s\n",
               response);


        if (strncmp(response,
                    "OK BYE",
                    6) == 0) {

            connected = 0;

            break;
        }


        printf("> ");

        fflush(stdout);
    }

    return NULL;
}


/* =========================
   MAIN CLIENT
   ========================= */

int main(int argc,
         char *argv[])
{
    int sockfd;

    struct sockaddr_in server_addr;

    char input[BUFFER_SIZE];


    if (argc != 2) {

        fprintf(stderr,
                "Usage: %s <server-ip>\n",
                argv[0]);

        exit(EXIT_FAILURE);
    }


    /* =========================
       CREATE SOCKET
       ========================= */

    sockfd =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (sockfd < 0) {

        perror("socket");

        exit(EXIT_FAILURE);
    }


    memset(&server_addr,
           0,
           sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);


    if (inet_pton(
            AF_INET,
            argv[1],
            &server_addr.sin_addr) <= 0) {

        fprintf(stderr,
                "Invalid server IP address\n");

        close(sockfd);

        exit(EXIT_FAILURE);
    }


    /* =========================
       CONNECT
       ========================= */

    if (connect(
            sockfd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0) {

        perror("connect");

        close(sockfd);

        exit(EXIT_FAILURE);
    }


    printf(
        "Connected to NetMessenger server at %s:%d\n",
        argv[1],
        PORT);


    printf(
        "First command must be: REGISTER <username>\n");


    /* =========================
       RECEIVE THREAD
       ========================= */

    pthread_t receiver_thread;


    if (pthread_create(
            &receiver_thread,
            NULL,
            receive_messages,
            &sockfd) != 0) {

        perror("pthread_create");

        close(sockfd);

        exit(EXIT_FAILURE);
    }


    /* =========================
       INPUT LOOP
       ========================= */

    while (connected) {

        printf("> ");

        fflush(stdout);


        if (fgets(input,
                  sizeof(input),
                  stdin) == NULL) {

            break;
        }


        size_t length =
            strlen(input);


        if (length == 0) {
            continue;
        }


        /*
         * Make sure normal protocol commands
         * end with newline.
         */
        if (input[length - 1] != '\n') {

            if (length <
                sizeof(input) - 1) {

                input[length] =
                    '\n';

                input[length + 1] =
                    '\0';

                length++;
            }
        }


        /* =====================
           SENDFILE
           ===================== */

        if (strncmp(input,
                    "SENDFILE ",
                    9) == 0) {

            char target[MAX_USERNAME];
            char filename[MAX_FILENAME];

            unsigned long long
                declared_size;

            char extra[2];


            int fields =
                sscanf(
                    input,
                    "SENDFILE %31s %255s %llu %1s",
                    target,
                    filename,
                    &declared_size,
                    extra);


            if (fields != 3) {

                printf(
                    "Usage: SENDFILE <target> <filename> <filesize>\n");

                continue;
            }


            FILE *file =
                fopen(filename,
                      "rb");


            if (file == NULL) {

                perror("fopen");

                continue;
            }


            /*
             * Find actual file size.
             */
            if (fseek(file,
                      0,
                      SEEK_END) != 0) {

                fclose(file);

                printf(
                    "Unable to determine file size.\n");

                continue;
            }


            long actual_size =
                ftell(file);


            if (actual_size < 0) {

                fclose(file);

                printf(
                    "Unable to determine file size.\n");

                continue;
            }


            rewind(file);


            /*
             * The command must contain
             * the correct filesize.
             */
            if ((unsigned long long)
                    actual_size
                != declared_size) {

                printf(
                    "Declared filesize does not match actual file size.\n");

                printf(
                    "Actual size: %ld bytes\n",
                    actual_size);

                fclose(file);

                continue;
            }


            /*
             * Send SENDFILE command line.
             */
            if (send_all(
                    sockfd,
                    input,
                    length) < 0) {

                fclose(file);

                perror("send");

                connected = 0;

                break;
            }


            /*
             * Send raw bytes immediately
             * after the SENDFILE line.
             */
            unsigned char chunk[4096];

            size_t bytes_read;


            while ((bytes_read =
                        fread(
                            chunk,
                            1,
                            sizeof(chunk),
                            file)) > 0) {

                if (send_all(
                        sockfd,
                        (const char *)chunk,
                        bytes_read) < 0) {

                    perror("send");

                    connected = 0;

                    break;
                }
            }


            if (ferror(file)) {

                printf(
                    "Error while reading file.\n");
            }


            fclose(file);


            /*
             * Prevent normal command sender
             * from sending SENDFILE line again.
             */
            continue;
        }


        /* =====================
           NORMAL COMMAND
           ===================== */

        if (send_all(
                sockfd,
                input,
                length) < 0) {

            perror("send");

            connected = 0;

            break;
        }


        if (strncmp(input,
                    "QUIT",
                    4) == 0) {

            break;
        }
    }


    shutdown(sockfd,
             SHUT_WR);


    pthread_join(receiver_thread,
                 NULL);


    close(sockfd);


    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 14630
#define BUFFER_SIZE 1024

volatile int connected = 1;

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

void *receive_messages(void *arg)
{
    int sockfd = *((int *)arg);

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

        printf("\n%s\n", response);

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

int main(int argc, char *argv[])
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

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  argv[1],
                  &server_addr.sin_addr) <= 0) {

        fprintf(stderr,
                "Invalid server IP address\n");

        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to NetMessenger server at %s:%d\n",
           argv[1],
           PORT);

    printf("First command must be: REGISTER <username>\n");

    pthread_t receiver_thread;

    if (pthread_create(&receiver_thread,
                       NULL,
                       receive_messages,
                       &sockfd) != 0) {

        perror("pthread_create");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

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

        if (input[length - 1] != '\n') {

            if (length < sizeof(input) - 1) {
                input[length] = '\n';
                input[length + 1] = '\0';
                length++;
            }
        }

        if (send_all(sockfd,
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

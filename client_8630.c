#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 14630
#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Create TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Prepare server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Connect to server */
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to NetMessenger server at %s:%d\n",
           argv[1], PORT);

    /* Receive test message */
    bytes_received = recv(sockfd,
                          buffer,
                          sizeof(buffer) - 1,
                          0);

    if (bytes_received < 0) {
        perror("recv");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (bytes_received == 0) {
        printf("Server closed the connection.\n");
    } else {
        buffer[bytes_received] = '\0';
        printf("Server says: %s", buffer);
    }

    close(sockfd);

    return 0;
}

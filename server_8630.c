#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 14630
#define MAX_CLIENTS 10
#define MAX_USERNAME 32
#define BUFFER_SIZE 1024
#define NID_TAG "NID:7186"

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME];
    int registered;
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Send all bytes in a buffer */
int send_all(int sockfd, const char *buffer, size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length) {
        ssize_t sent = send(sockfd,
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

/* Read exactly one newline-terminated command */
ssize_t read_line(int sockfd, char *buffer, size_t max_length)
{
    size_t index = 0;

    while (index < max_length - 1) {
        char ch;

        ssize_t received = recv(sockfd, &ch, 1, 0);

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

/* Find an empty slot in the client table */
int add_client(int socket_fd)
{
    int index = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket_fd == 0) {
            clients[i].socket_fd = socket_fd;
            clients[i].username[0] = '\0';
            clients[i].registered = 0;

            index = i;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return index;
}

/* Remove client from the table */
void remove_client(int index)
{
    pthread_mutex_lock(&clients_mutex);

    clients[index].socket_fd = 0;
    clients[index].username[0] = '\0';
    clients[index].registered = 0;

    pthread_mutex_unlock(&clients_mutex);
}

/* Check whether username already exists */
int username_exists(const char *username)
{
    int exists = 0;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].registered &&
            strcmp(clients[i].username, username) == 0) {

            exists = 1;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return exists;
}

/* Build the LIST response */
void send_user_list(int client_fd)
{
    char response[BUFFER_SIZE];

    strcpy(response, "OK USERS ");

    pthread_mutex_lock(&clients_mutex);

    int first = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].registered) {
            if (!first) {
                strcat(response, ",");
            }

            strcat(response, clients[i].username);
            first = 0;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strcat(response, " " NID_TAG "\n");

    send_all(client_fd, response, strlen(response));
}

/* Handle one connected client */
void *handle_client(void *arg)
{
    int index = *((int *)arg);
    free(arg);

    int client_fd = clients[index].socket_fd;

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    /*
     * REGISTER must be the first command.
     */
    ssize_t bytes = read_line(client_fd, buffer, sizeof(buffer));

    if (bytes <= 0) {
        close(client_fd);
        remove_client(index);
        return NULL;
    }

    if (strncmp(buffer, "REGISTER ", 9) != 0) {
        snprintf(response,
                 sizeof(response),
                 "ERR 005 REGISTER_REQUIRED %s\n",
                 NID_TAG);

        send_all(client_fd, response, strlen(response));

        close(client_fd);
        remove_client(index);
        return NULL;
    }

    char *username = buffer + 9;

    if (strlen(username) == 0 ||
        strlen(username) >= MAX_USERNAME) {

        snprintf(response,
                 sizeof(response),
                 "ERR 005 INVALID_USERNAME %s\n",
                 NID_TAG);

        send_all(client_fd, response, strlen(response));

        close(client_fd);
        remove_client(index);
        return NULL;
    }

    if (username_exists(username)) {

        snprintf(response,
                 sizeof(response),
                 "ERR 001 USERNAME_TAKEN %s\n",
                 NID_TAG);

        send_all(client_fd, response, strlen(response));

        close(client_fd);
        remove_client(index);
        return NULL;
    }

    pthread_mutex_lock(&clients_mutex);

    strncpy(clients[index].username,
            username,
            MAX_USERNAME - 1);

    clients[index].username[MAX_USERNAME - 1] = '\0';
    clients[index].registered = 1;

    pthread_mutex_unlock(&clients_mutex);

    snprintf(response,
             sizeof(response),
             "OK REGISTERED %s %s\n",
             username,
             NID_TAG);

    send_all(client_fd, response, strlen(response));

    printf("User registered: %s\n", username);

    /*
     * Continue accepting commands from this client.
     */
    while (1) {

        bytes = read_line(client_fd,
                          buffer,
                          sizeof(buffer));

        if (bytes == 0) {
            printf("Client disconnected: %s\n",
                   clients[index].username);
            break;
        }

        if (bytes < 0) {
            perror("recv");
            break;
        }

        if (strcmp(buffer, "LIST") == 0) {

            send_user_list(client_fd);

        } else if (strcmp(buffer, "QUIT") == 0) {

            snprintf(response,
                     sizeof(response),
                     "OK BYE %s\n",
                     NID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));

            break;

        } else {

            snprintf(response,
                     sizeof(response),
                     "ERR 005 INVALID_COMMAND %s\n",
                     NID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));
        }
    }

    close(client_fd);
    remove_client(index);

    return NULL;
}

int main(void)
{
    int server_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    /* Initialise client table */
    memset(clients, 0, sizeof(clients));

    /* Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /*
     * Allows quick server restart without waiting
     * for the old socket state to disappear.
     */
    int option = 1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &option,
               sizeof(option));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {

        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("NetMessenger server started.\n");
    printf("Listening on 0.0.0.0:%d\n", PORT);
    printf("Node ID: %s\n", NID_TAG);

    while (1) {

        client_len = sizeof(client_addr);

        int client_fd =
            accept(server_fd,
                   (struct sockaddr *)&client_addr,
                   &client_len);

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("New connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        int index = add_client(client_fd);

        if (index < 0) {
            const char *full_message =
                "ERR 006 SERVER_FULL " NID_TAG "\n";

            send_all(client_fd,
                     full_message,
                     strlen(full_message));

            close(client_fd);
            continue;
        }

        pthread_t thread;

        int *thread_index = malloc(sizeof(int));

        if (thread_index == NULL) {
            perror("malloc");
            close(client_fd);
            remove_client(index);
            continue;
        }

        *thread_index = index;

        if (pthread_create(&thread,
                           NULL,
                           handle_client,
                           thread_index) != 0) {

            perror("pthread_create");

            free(thread_index);
            close(client_fd);
            remove_client(index);

            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}

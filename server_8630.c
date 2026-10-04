#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 14630

#define MAX_CLIENTS 10
#define MAX_USERNAME 32

#define MAX_ROOMS 20
#define MAX_ROOM_NAME 32

#define MAX_FILENAME 256
#define BUFFER_SIZE 1024

#define MAX_FILE_SIZE (10 * 1024 * 1024)

#define NID_TAG "NID:7186"

#define STORAGE_ROOT "./storage/IT23718630"


/* =========================
   DATA STRUCTURES
   ========================= */

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME];
    int registered;
} Client;


typedef struct {
    int in_use;
    char name[MAX_ROOM_NAME];
    int members[MAX_CLIENTS];
} Room;


Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;


/* =========================
   BASIC SOCKET HELPERS
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


/*
 * Read one newline-terminated text command.
 */
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


/*
 * Receive exactly 'length' raw bytes.
 * Used for SENDFILE.
 */
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


/*
 * Consume exactly 'length' bytes without saving them.
 * This protects TCP framing when a file is rejected.
 */
int discard_exact(int sockfd,
                  size_t length)
{
    unsigned char temp[4096];

    size_t remaining = length;

    while (remaining > 0) {

        size_t chunk;

        if (remaining < sizeof(temp)) {
            chunk = remaining;
        } else {
            chunk = sizeof(temp);
        }

        ssize_t received =
            recv(sockfd,
                 temp,
                 chunk,
                 0);

        if (received <= 0) {
            return -1;
        }

        remaining -= (size_t)received;
    }

    return 0;
}


/* =========================
   VALIDATION
   ========================= */

int valid_name(const char *name,
               size_t max_length)
{
    size_t length = strlen(name);

    if (length == 0 ||
        length >= max_length) {

        return 0;
    }

    for (size_t i = 0;
         i < length;
         i++) {

        if (name[i] == ' ' ||
            name[i] == '\t') {

            return 0;
        }
    }

    return 1;
}


int valid_filename(const char *filename)
{
    if (filename == NULL) {
        return 0;
    }

    size_t length =
        strlen(filename);

    if (length == 0 ||
        length >= MAX_FILENAME) {

        return 0;
    }

    /*
     * Prevent path traversal.
     */
    if (strstr(filename, "..") != NULL ||
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL) {

        return 0;
    }

    return 1;
}


/* =========================
   CLIENT MANAGEMENT
   ========================= */

int add_client(int socket_fd)
{
    int index = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i].socket_fd == 0) {

            clients[i].socket_fd =
                socket_fd;

            clients[i].username[0] =
                '\0';

            clients[i].registered =
                0;

            index = i;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return index;
}


void remove_client_from_rooms(int client_index)
{
    pthread_mutex_lock(&rooms_mutex);

    for (int i = 0;
         i < MAX_ROOMS;
         i++) {

        if (rooms[i].in_use) {

            rooms[i].members[client_index] =
                0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}


void remove_client(int index)
{
    remove_client_from_rooms(index);

    pthread_mutex_lock(&clients_mutex);

    clients[index].socket_fd =
        0;

    clients[index].username[0] =
        '\0';

    clients[index].registered =
        0;

    pthread_mutex_unlock(&clients_mutex);
}


int register_username(int index,
                      const char *username)
{
    int success = 1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i].registered &&
            strcmp(clients[i].username,
                   username) == 0) {

            success = 0;
            break;
        }
    }

    if (success) {

        strncpy(clients[index].username,
                username,
                MAX_USERNAME - 1);

        clients[index]
            .username[MAX_USERNAME - 1] =
            '\0';

        clients[index].registered =
            1;
    }

    pthread_mutex_unlock(&clients_mutex);

    return success;
}


int find_user_socket(const char *username)
{
    int socket_fd = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i].registered &&
            strcmp(clients[i].username,
                   username) == 0) {

            socket_fd =
                clients[i].socket_fd;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return socket_fd;
}


void send_user_list(int client_fd)
{
    char response[BUFFER_SIZE];

    strcpy(response,
           "OK USERS ");

    pthread_mutex_lock(&clients_mutex);

    int first = 1;

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i].registered) {

            if (!first) {
                strcat(response, ",");
            }

            strcat(response,
                   clients[i].username);

            first = 0;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strcat(response,
           " " NID_TAG "\n");

    send_all(client_fd,
             response,
             strlen(response));
}


/* =========================
   BROADCAST MESSAGE
   ========================= */

void broadcast_message(int sender_index,
                       const char *message)
{
    int target_sockets[MAX_CLIENTS];

    int target_count = 0;

    char sender[MAX_USERNAME];

    pthread_mutex_lock(&clients_mutex);

    strncpy(sender,
            clients[sender_index].username,
            sizeof(sender) - 1);

    sender[sizeof(sender) - 1] =
        '\0';

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (i != sender_index &&
            clients[i].registered) {

            target_sockets[target_count++] =
                clients[i].socket_fd;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    char outgoing[BUFFER_SIZE];

    snprintf(outgoing,
             sizeof(outgoing),
             "MSG BCAST %s %s\n",
             sender,
             message);

    for (int i = 0;
         i < target_count;
         i++) {

        send_all(target_sockets[i],
                 outgoing,
                 strlen(outgoing));
    }
}


/* =========================
   PRIVATE MESSAGE
   ========================= */

int private_message(int sender_index,
                    const char *target_username,
                    const char *message)
{
    int target_fd = -1;

    char sender[MAX_USERNAME];

    pthread_mutex_lock(&clients_mutex);

    strncpy(sender,
            clients[sender_index].username,
            sizeof(sender) - 1);

    sender[sizeof(sender) - 1] =
        '\0';

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i].registered &&
            strcmp(clients[i].username,
                   target_username) == 0) {

            target_fd =
                clients[i].socket_fd;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    if (target_fd < 0) {
        return 0;
    }

    char outgoing[BUFFER_SIZE];

    snprintf(outgoing,
             sizeof(outgoing),
             "MSG PRIV %s %s\n",
             sender,
             message);

    if (send_all(target_fd,
                 outgoing,
                 strlen(outgoing)) < 0) {

        return 0;
    }

    return 1;
}


/* =========================
   ROOM MANAGEMENT
   ========================= */

int find_room_locked(const char *room_name)
{
    for (int i = 0;
         i < MAX_ROOMS;
         i++) {

        if (rooms[i].in_use &&
            strcmp(rooms[i].name,
                   room_name) == 0) {

            return i;
        }
    }

    return -1;
}


int join_room(int client_index,
              const char *room_name)
{
    int room_index;

    pthread_mutex_lock(&rooms_mutex);

    room_index =
        find_room_locked(room_name);

    /*
     * Create room if it does not exist.
     */
    if (room_index < 0) {

        for (int i = 0;
             i < MAX_ROOMS;
             i++) {

            if (!rooms[i].in_use) {

                rooms[i].in_use = 1;

                strncpy(rooms[i].name,
                        room_name,
                        MAX_ROOM_NAME - 1);

                rooms[i]
                    .name[MAX_ROOM_NAME - 1] =
                    '\0';

                memset(rooms[i].members,
                       0,
                       sizeof(rooms[i].members));

                room_index = i;

                break;
            }
        }
    }

    if (room_index >= 0) {

        rooms[room_index]
            .members[client_index] =
            1;
    }

    pthread_mutex_unlock(&rooms_mutex);

    return room_index >= 0;
}


int leave_room(int client_index,
               const char *room_name)
{
    int result = 0;

    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room_locked(room_name);

    if (room_index < 0) {

        result = -1;
    }

    else if (!rooms[room_index]
                 .members[client_index]) {

        result = 0;
    }

    else {

        rooms[room_index]
            .members[client_index] =
            0;

        result = 1;
    }

    pthread_mutex_unlock(&rooms_mutex);

    return result;
}


void send_room_list(int client_fd)
{
    char response[BUFFER_SIZE];

    strcpy(response,
           "OK ROOMS ");

    pthread_mutex_lock(&rooms_mutex);

    int first = 1;

    for (int i = 0;
         i < MAX_ROOMS;
         i++) {

        if (rooms[i].in_use) {

            if (!first) {
                strcat(response, ",");
            }

            strcat(response,
                   rooms[i].name);

            first = 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);

    strcat(response,
           " " NID_TAG "\n");

    send_all(client_fd,
             response,
             strlen(response));
}


int room_message(int sender_index,
                 const char *room_name,
                 const char *message)
{
    int target_sockets[MAX_CLIENTS];

    int target_count = 0;

    char sender[MAX_USERNAME];

    pthread_mutex_lock(&clients_mutex);

    strncpy(sender,
            clients[sender_index].username,
            sizeof(sender) - 1);

    sender[sizeof(sender) - 1] =
        '\0';

    pthread_mutex_unlock(&clients_mutex);

    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room_locked(room_name);

    if (room_index < 0) {

        pthread_mutex_unlock(&rooms_mutex);

        return -1;
    }

    if (!rooms[room_index]
             .members[sender_index]) {

        pthread_mutex_unlock(&rooms_mutex);

        return 0;
    }

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (i != sender_index &&
            rooms[room_index].members[i] &&
            clients[i].registered) {

            target_sockets[target_count++] =
                clients[i].socket_fd;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_unlock(&rooms_mutex);

    char outgoing[BUFFER_SIZE];

    snprintf(outgoing,
             sizeof(outgoing),
             "MSG ROOM %s %s %s\n",
             room_name,
             sender,
             message);

    for (int i = 0;
         i < target_count;
         i++) {

        send_all(target_sockets[i],
                 outgoing,
                 strlen(outgoing));
    }

    return 1;
}


/* =========================
   FILE STORAGE
   ========================= */

int create_storage_directories(const char *sender)
{
    char sender_path[512];

    if (mkdir("./storage", 0755) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    if (mkdir(STORAGE_ROOT, 0755) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    snprintf(sender_path,
             sizeof(sender_path),
             "%s/%s",
             STORAGE_ROOT,
             sender);

    if (mkdir(sender_path, 0755) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    return 0;
}


int save_file_copy(const char *sender,
                   const char *filename,
                   const unsigned char *data,
                   size_t file_size)
{
    char path[768];

    if (create_storage_directories(sender) < 0) {
        return -1;
    }

    snprintf(path,
             sizeof(path),
             "%s/%s/%s",
             STORAGE_ROOT,
             sender,
             filename);

    FILE *file =
        fopen(path, "wb");

    if (file == NULL) {
        return -1;
    }

    size_t written = 0;

    if (file_size > 0) {

        written =
            fwrite(data,
                   1,
                   file_size,
                   file);
    }

    fclose(file);

    if (written != file_size) {
        return -1;
    }

    return 0;
}


/* =========================
   FILE TARGET HELPERS
   ========================= */

int get_room_targets(int sender_index,
                     const char *room_name,
                     int target_sockets[],
                     int *target_count)
{
    *target_count = 0;

    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room_locked(room_name);

    if (room_index < 0) {

        pthread_mutex_unlock(&rooms_mutex);

        return -1;
    }

    if (!rooms[room_index]
             .members[sender_index]) {

        pthread_mutex_unlock(&rooms_mutex);

        return 0;
    }

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (i != sender_index &&
            rooms[room_index].members[i] &&
            clients[i].registered) {

            target_sockets[*target_count] =
                clients[i].socket_fd;

            (*target_count)++;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_unlock(&rooms_mutex);

    return 1;
}


/*
 * Receiver-side implementation extension:
 *
 * MSG FILE <sender> <filename> <filesize>\n
 * followed by exactly <filesize> raw bytes.
 */
int send_file_to_socket(int socket_fd,
                        const char *sender,
                        const char *filename,
                        size_t file_size,
                        const unsigned char *data)
{
    char header[BUFFER_SIZE];

    snprintf(header,
             sizeof(header),
             "MSG FILE %s %s %zu\n",
             sender,
             filename,
             file_size);

    if (send_all(socket_fd,
                 header,
                 strlen(header)) < 0) {

        return -1;
    }

    if (file_size > 0) {

        if (send_all(socket_fd,
                     (const char *)data,
                     file_size) < 0) {

            return -1;
        }
    }

    return 0;
}


/* =========================
   CLIENT THREAD
   ========================= */

void *handle_client(void *arg)
{
    int index =
        *((int *)arg);

    free(arg);

    int client_fd =
        clients[index].socket_fd;

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];


    /* =========================
       REGISTER MUST BE FIRST
       ========================= */

    ssize_t bytes =
        read_line(client_fd,
                  buffer,
                  sizeof(buffer));

    if (bytes <= 0) {

        close(client_fd);
        remove_client(index);

        return NULL;
    }


    if (strncmp(buffer,
                "REGISTER ",
                9) != 0) {

        snprintf(response,
                 sizeof(response),
                 "ERR 005 REGISTER_REQUIRED %s\n",
                 NID_TAG);

        send_all(client_fd,
                 response,
                 strlen(response));

        close(client_fd);
        remove_client(index);

        return NULL;
    }


    char *username =
        buffer + 9;


    if (!valid_name(username,
                    MAX_USERNAME)) {

        snprintf(response,
                 sizeof(response),
                 "ERR 005 INVALID_USERNAME %s\n",
                 NID_TAG);

        send_all(client_fd,
                 response,
                 strlen(response));

        close(client_fd);
        remove_client(index);

        return NULL;
    }


    if (!register_username(index,
                           username)) {

        snprintf(response,
                 sizeof(response),
                 "ERR 001 USERNAME_TAKEN %s\n",
                 NID_TAG);

        send_all(client_fd,
                 response,
                 strlen(response));

        close(client_fd);
        remove_client(index);

        return NULL;
    }


    snprintf(response,
             sizeof(response),
             "OK REGISTERED %s %s\n",
             username,
             NID_TAG);

    send_all(client_fd,
             response,
             strlen(response));


    printf("User registered: %s\n",
           username);


    /* =========================
       COMMAND LOOP
       ========================= */

    while (1) {

        bytes =
            read_line(client_fd,
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


        /* =====================
           LIST
           ===================== */

        if (strcmp(buffer,
                   "LIST") == 0) {

            send_user_list(client_fd);
        }


        /* =====================
           ROOMS
           ===================== */

        else if (strcmp(buffer,
                        "ROOMS") == 0) {

            send_room_list(client_fd);
        }


        /* =====================
           BCAST
           ===================== */

        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0) {

            char *message =
                buffer + 6;


            if (strlen(message) == 0) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_FORMAT %s\n",
                         NID_TAG);
            }

            else {

                broadcast_message(index,
                                  message);

                snprintf(response,
                         sizeof(response),
                         "OK SENT %s\n",
                         NID_TAG);
            }


            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* =====================
           PMSG
           ===================== */

        else if (strncmp(buffer,
                         "PMSG ",
                         5) == 0) {

            char *content =
                buffer + 5;

            char *space =
                strchr(content,
                       ' ');


            if (space == NULL) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_FORMAT %s\n",
                         NID_TAG);
            }

            else {

                *space = '\0';

                char *target_username =
                    content;

                char *message =
                    space + 1;


                if (strlen(target_username) == 0 ||
                    strlen(message) == 0) {

                    snprintf(response,
                             sizeof(response),
                             "ERR 005 INVALID_FORMAT %s\n",
                             NID_TAG);
                }

                else if (!private_message(
                              index,
                              target_username,
                              message)) {

                    snprintf(response,
                             sizeof(response),
                             "ERR 002 USER_NOT_FOUND %s\n",
                             NID_TAG);
                }

                else {

                    snprintf(response,
                             sizeof(response),
                             "OK SENT %s\n",
                             NID_TAG);
                }
            }


            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* =====================
           JOIN
           ===================== */

        else if (strncmp(buffer,
                         "JOIN ",
                         5) == 0) {

            char *room_name =
                buffer + 5;


            if (!valid_name(room_name,
                            MAX_ROOM_NAME)) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_ROOM_NAME %s\n",
                         NID_TAG);
            }

            else if (!join_room(index,
                                room_name)) {

                snprintf(response,
                         sizeof(response),
                         "ERR 006 SERVER_FULL %s\n",
                         NID_TAG);
            }

            else {

                snprintf(response,
                         sizeof(response),
                         "OK JOINED %s %s\n",
                         room_name,
                         NID_TAG);
            }


            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* =====================
           LEAVE
           ===================== */

        else if (strncmp(buffer,
                         "LEAVE ",
                         6) == 0) {

            char *room_name =
                buffer + 6;


            int result =
                leave_room(index,
                           room_name);


            if (result == -1) {

                snprintf(response,
                         sizeof(response),
                         "ERR 003 ROOM_NOT_FOUND %s\n",
                         NID_TAG);
            }

            else if (result == 0) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 NOT_IN_ROOM %s\n",
                         NID_TAG);
            }

            else {

                snprintf(response,
                         sizeof(response),
                         "OK LEFT %s %s\n",
                         room_name,
                         NID_TAG);
            }


            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* =====================
           RMSG
           ===================== */

        else if (strncmp(buffer,
                         "RMSG ",
                         5) == 0) {

            char *content =
                buffer + 5;

            char *space =
                strchr(content,
                       ' ');


            if (space == NULL) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_FORMAT %s\n",
                         NID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            *space = '\0';


            char *room_name =
                content;

            char *message =
                space + 1;


            if (strlen(room_name) == 0 ||
                strlen(message) == 0) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_FORMAT %s\n",
                         NID_TAG);
            }

            else {

                int result =
                    room_message(index,
                                 room_name,
                                 message);


                if (result == -1) {

                    snprintf(response,
                             sizeof(response),
                             "ERR 003 ROOM_NOT_FOUND %s\n",
                             NID_TAG);
                }

                else if (result == 0) {

                    snprintf(response,
                             sizeof(response),
                             "ERR 005 NOT_IN_ROOM %s\n",
                             NID_TAG);
                }

                else {

                    snprintf(response,
                             sizeof(response),
                             "OK SENT %s\n",
                             NID_TAG);
                }
            }


            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* =====================
           SENDFILE
           ===================== */

        else if (strncmp(buffer,
                         "SENDFILE ",
                         9) == 0) {

            char target[MAX_USERNAME];
            char filename[MAX_FILENAME];

            unsigned long long
                file_size_value;

            char extra[2];


            int fields =
                sscanf(buffer,
                       "SENDFILE %31s %255s %llu %1s",
                       target,
                       filename,
                       &file_size_value,
                       extra);


            if (fields != 3) {

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_FORMAT %s\n",
                         NID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            /*
             * Prevent values larger than size_t.
             */
            if (file_size_value >
                (unsigned long long)MAX_FILE_SIZE) {

                if (discard_exact(
                        client_fd,
                        (size_t)file_size_value) < 0) {

                    break;
                }

                snprintf(response,
                         sizeof(response),
                         "ERR 004 FILE_TOO_LARGE %s\n",
                         NID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            size_t file_size =
                (size_t)file_size_value;


            if (!valid_filename(filename)) {

                if (discard_exact(client_fd,
                                  file_size) < 0) {

                    break;
                }

                snprintf(response,
                         sizeof(response),
                         "ERR 005 INVALID_FILENAME %s\n",
                         NID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            /*
             * Resolve target before storing the file.
             *
             * Priority:
             * 1. Username
             * 2. Room name
             */
            int user_socket =
                find_user_socket(target);


            int target_sockets[MAX_CLIENTS];

            int target_count = 0;

            int room_result = -1;


            if (user_socket < 0) {

                room_result =
                    get_room_targets(
                        index,
                        target,
                        target_sockets,
                        &target_count);


                if (room_result == -1) {

                    /*
                     * Target does not match
                     * a user or an existing room.
                     *
                     * We must still consume
                     * the raw file bytes.
                     */
                    if (discard_exact(
                            client_fd,
                            file_size) < 0) {

                        break;
                    }

                    snprintf(
                        response,
                        sizeof(response),
                        "ERR 002 USER_NOT_FOUND %s\n",
                        NID_TAG);

                    send_all(
                        client_fd,
                        response,
                        strlen(response));

                    continue;
                }


                if (room_result == 0) {

                    if (discard_exact(
                            client_fd,
                            file_size) < 0) {

                        break;
                    }

                    snprintf(
                        response,
                        sizeof(response),
                        "ERR 005 NOT_IN_ROOM %s\n",
                        NID_TAG);

                    send_all(
                        client_fd,
                        response,
                        strlen(response));

                    continue;
                }
            }


            unsigned char *file_data =
                NULL;


            if (file_size > 0) {

                file_data =
                    malloc(file_size);


                if (file_data == NULL) {

                    if (discard_exact(
                            client_fd,
                            file_size) < 0) {

                        break;
                    }

                    snprintf(
                        response,
                        sizeof(response),
                        "ERR 006 SERVER_ERROR %s\n",
                        NID_TAG);

                    send_all(
                        client_fd,
                        response,
                        strlen(response));

                    continue;
                }


                if (receive_exact(
                        client_fd,
                        file_data,
                        file_size) < 0) {

                    free(file_data);

                    break;
                }
            }


            char sender[MAX_USERNAME];


            pthread_mutex_lock(
                &clients_mutex);


            strncpy(
                sender,
                clients[index].username,
                sizeof(sender) - 1);


            sender[sizeof(sender) - 1] =
                '\0';


            pthread_mutex_unlock(
                &clients_mutex);


            /*
             * Store a server-side copy.
             */
            if (save_file_copy(
                    sender,
                    filename,
                    file_data,
                    file_size) < 0) {

                free(file_data);

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 006 SERVER_ERROR %s\n",
                    NID_TAG);

                send_all(
                    client_fd,
                    response,
                    strlen(response));

                continue;
            }


            /*
             * User target.
             */
            if (user_socket >= 0) {

                send_file_to_socket(
                    user_socket,
                    sender,
                    filename,
                    file_size,
                    file_data);
            }


            /*
             * Room target.
             */
            else {

                for (int i = 0;
                     i < target_count;
                     i++) {

                    send_file_to_socket(
                        target_sockets[i],
                        sender,
                        filename,
                        file_size,
                        file_data);
                }
            }


            free(file_data);


            snprintf(
                response,
                sizeof(response),
                "OK FILE_RECEIVED %s %s\n",
                filename,
                NID_TAG);


            send_all(
                client_fd,
                response,
                strlen(response));
        }


        /* =====================
           QUIT
           ===================== */

        else if (strcmp(buffer,
                        "QUIT") == 0) {

            snprintf(response,
                     sizeof(response),
                     "OK BYE %s\n",
                     NID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));

            break;
        }


        /* =====================
           INVALID COMMAND
           ===================== */

        else {

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


/* =========================
   MAIN SERVER
   ========================= */

int main(void)
{
    int server_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;


    /*
     * Prevent server termination
     * if sending to a disconnected client.
     */
    signal(SIGPIPE,
           SIG_IGN);


    memset(clients,
           0,
           sizeof(clients));

    memset(rooms,
           0,
           sizeof(rooms));


    /* =========================
       CREATE SOCKET
       ========================= */

    server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (server_fd < 0) {

        perror("socket");

        exit(EXIT_FAILURE);
    }


    /*
     * Allow quick server restart.
     */
    int option = 1;


    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)) < 0) {

        perror("setsockopt");

        close(server_fd);

        exit(EXIT_FAILURE);
    }


    memset(&server_addr,
           0,
           sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);


    /* =========================
       BIND
       ========================= */

    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0) {

        perror("bind");

        close(server_fd);

        exit(EXIT_FAILURE);
    }


    /* =========================
       LISTEN
       ========================= */

    if (listen(server_fd,
               MAX_CLIENTS) < 0) {

        perror("listen");

        close(server_fd);

        exit(EXIT_FAILURE);
    }


    printf("NetMessenger server started.\n");

    printf("Listening on 0.0.0.0:%d\n",
           PORT);

    printf("Node ID: %s\n",
           NID_TAG);


    /* =========================
       ACCEPT CLIENTS
       ========================= */

    while (1) {

        client_len =
            sizeof(client_addr);


        int client_fd =
            accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_len);


        if (client_fd < 0) {

            perror("accept");

            continue;
        }


        printf(
            "New connection from %s:%d\n",
            inet_ntoa(
                client_addr.sin_addr),
            ntohs(
                client_addr.sin_port));


        int index =
            add_client(client_fd);


        if (index < 0) {

            const char *full_message =
                "ERR 006 SERVER_FULL "
                NID_TAG
                "\n";


            send_all(
                client_fd,
                full_message,
                strlen(full_message));


            close(client_fd);

            continue;
        }


        pthread_t thread;


        int *thread_index =
            malloc(sizeof(int));


        if (thread_index == NULL) {

            perror("malloc");

            close(client_fd);

            remove_client(index);

            continue;
        }


        *thread_index =
            index;


        if (pthread_create(
                &thread,
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

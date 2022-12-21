#ifndef CFOSSIL_FOSSIL_H
#define CFOSSIL_FOSSIL_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#define PROTOCOL_VERSION "v1.0.0"

#define COMMAND_LEN 8

typedef struct fossil_client {
    int fd;
    int socket_fd;
    struct sockaddr_in addr;
} fossil_client_t;

typedef struct fossil_message {
    char command[8];
    uint32_t len;
    void *data;
} fossil_message_t;

void fossil_message_free(fossil_message_t *message) {
    if (message->data != NULL) {
        free(message->data);
        message->data = NULL;
        message->len = 0;
    }
}

ssize_t fossil_read_message(fossil_client_t *client, fossil_message_t *message) {
    ssize_t result = 0;
    // First, read the length of the message, which is the first 4 bytes
    uint32_t len = 0;
    if ((result = read(client->socket_fd, &len, 4)) <= 0)
        return result;

    // If our overall length is less than the command length, we have a problem
    if (len < COMMAND_LEN)
        return -1;

    if ((result = read(client->socket_fd, message->command, 8)) < 8)
        return -1;

    message->len = len - COMMAND_LEN;
    message->data = malloc(len - COMMAND_LEN);
    result += read(client->socket_fd, message->data, len - COMMAND_LEN);

    return result;
}

int fossil_connect(fossil_client_t *client) {
    int result = 0;

    if ((client->socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    client->addr.sin_family = AF_INET;
    // FIXME: Generalize port
    client->addr.sin_port = htons(8001);

    // FIXME: Generalize IP address
    if (inet_pton(AF_INET, "127.0.0.1", &client->addr.sin_addr) <= 0) {
        return -1;
    }

    if ((client->fd = connect(client->socket_fd, (struct sockaddr*)&client->addr, sizeof(client->addr))) < 0) {
        return -1;
    }

    // Now that we're connected, send a version advertisement
    char version[19];
    uint32_t len = 14;
    memcpy(version, &len, 4);
    sprintf(version + 4, "VERSION\0%s", PROTOCOL_VERSION);
    send(client->socket_fd, version, 18, 0);


    fossil_message_t server_version;
    len = fossil_read_message(client, &server_version);

    if (strcmp(server_version.command, "ERR") == 0) {
        uint32_t *error_code = (uint32_t *)server_version.data;
        char *message = (char *)server_version.data + 4;
        printf("%d %s", *error_code, message);
        result = -1;
        goto cleanup;
    }

    uint32_t *code = (uint32_t *)server_version.data;
    char *s_version = (char *)server_version.data + 4;

    printf("%d %s", *code, s_version);

cleanup:
    fossil_message_free(&server_version);
    return result;
}

#endif //CFOSSIL_FOSSIL_H

#ifndef CFOSSIL_FOSSIL_H
#define CFOSSIL_FOSSIL_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#define PROTOCOL_VERSION "v1.0.0"

struct fossil_client_t {
    int fd;
    int socket_fd;
    struct sockaddr_in addr;
};

int fossil_connect(struct fossil_client_t *client) {
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

    len = 0;
    read(client->socket_fd, &len, 4);

    char command[8];
    read(client->socket_fd, command, 8);

    if (strcmp(command, "ERR") == 0) {
        uint32_t error_code;
        read(client->socket_fd, &error_code, 4);
        char *message = malloc(len - 8 - 4);
        read(client->socket_fd, message, len - 8 - 4);
        free(message);
        return -1;
    }

    char *server_version = malloc(len - 8 - 4);
    uint32_t code;

    read(client->socket_fd, &code, 4);
    read(client->socket_fd, server_version, 6);
    printf("%d %s", code, server_version);
    free(server_version);

    return 0;
    }

#endif //CFOSSIL_FOSSIL_H

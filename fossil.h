#ifndef CFOSSIL_FOSSIL_H
#define CFOSSIL_FOSSIL_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#define PROTOCOL_VERSION "v1.0.0"

#define COMMAND_LEN 8

typedef struct fossil_client
{
    int fd;
    int socket_fd;
    struct sockaddr_in addr;
} fossil_client_t;

typedef struct fossil_message
{
    char command[8];
    uint32_t len;
    void *data;
} fossil_message_t;

typedef struct fossil_request
{
    enum {
        FOSSIL_REQ_VERSION
    } type;
} fossil_request_t;

typedef struct fossil_response
{
    enum {
        FOSSIL_RESP_UNKNOWN,
        FOSSIL_RESP_ERR,
        FOSSIL_RESP_VERSION,
    } type;
    uint32_t code;
    char *explanation;
} fossil_response_t;

typedef struct fossil_version_request
{
    fossil_request_t base;
    char *version;
} fossil_version_request_t;

typedef struct fossil_version_response
{
    fossil_response_t base;
    char *version;
} fossil_version_response_t;

fossil_message_t fossil_request_marshal(fossil_request_t *req)
{
    fossil_message_t message;

    size_t len = 0;
    fossil_version_request_t *version_req;

    switch (req->type)
    {
        case FOSSIL_REQ_VERSION:
            version_req = (fossil_version_request_t *)req;
            strcpy(message.command, "VERSION");
            len = strlen(version_req->version) + 1;
            message.data = malloc(len);
            strcpy(message.data, version_req->version);
            message.len = len;
    }

    return message;
}

fossil_response_t *fossil_response_unmarshal(fossil_message_t *message)
{
    if (strcmp(message->command, "VERSION") == 0)
    {
        fossil_version_response_t *version_response = malloc(sizeof(fossil_version_response_t));
        version_response->base.type = FOSSIL_RESP_VERSION;
        version_response->base.code = *((uint32_t *)message->data);
        version_response->version = malloc(message->len - 4);
        strcpy(version_response->version, (char *)message->data + 4);
        return (fossil_response_t *)version_response;
    }

    if (strcmp(message->command, "ERR") == 0)
    {
        fossil_response_t *err_response = malloc(sizeof(fossil_response_t));
        err_response->type = FOSSIL_RESP_ERR;
        err_response->code = *((uint32_t *)message->data);
        err_response->explanation = malloc(message->len - 4);
        strcpy(err_response->explanation, (char *)message->data + 4);
        return err_response;
    }

    return NULL;
}


void fossil_message_free(fossil_message_t *message)
{
    if (message->data != NULL) {
        free(message->data);
        message->data = NULL;
        message->len = 0;
    }
}

ssize_t fossil_read_message(fossil_client_t *client, fossil_message_t *message)
{
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

ssize_t fossil_write_message(fossil_client_t *client, fossil_message_t *message)
{
    ssize_t result;
    // First, write the length of the message
    uint32_t len = COMMAND_LEN + message->len;
    if ((result = write(client->socket_fd, &len, 4)) < 0)
        return result;

    // Now, write out the command
    if ((result = write(client->socket_fd, message->command, 8)) < 0)
        return result;

    // Finally, write out our data
    if ((result = write(client->socket_fd, message->data, message->len)) < 0)
        return result;

    return len;
}

ssize_t fossil_connect(fossil_client_t *client)
{
    ssize_t result = 0;

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
    fossil_version_request_t advertisement = {.base.type = FOSSIL_REQ_VERSION};
    advertisement.version = PROTOCOL_VERSION;

    fossil_message_t msg_advertisement = fossil_request_marshal((fossil_request_t *)&advertisement);
    if ((result = fossil_write_message(client, &msg_advertisement)) < 0)
        goto cleanup;


    fossil_message_t response;
    fossil_read_message(client, &response);
    fossil_version_response_t *server_version = (fossil_version_response_t  *)fossil_response_unmarshal(&response);

    if (server_version == NULL)
        goto cleanup;

    if (server_version->base.type == FOSSIL_RESP_ERR)
    {
        result = -1;
        printf("%d %s", server_version->base.code, server_version->base.explanation);
        goto cleanup;
    }

    printf("%d %s", server_version->base.code, server_version->version);

cleanup:
    fossil_message_free(&msg_advertisement);
    return result;
}

#endif //CFOSSIL_FOSSIL_H

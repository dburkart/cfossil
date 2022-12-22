/**
 * Copyright 2022 Dana Burkart <dana.burkart@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
        FOSSIL_REQ_VERSION,
        FOSSIL_REQ_USE,
        FOSSIL_REQ_APPEND,
    } type;
} fossil_request_t;

typedef struct fossil_response
{
    enum {
        FOSSIL_RESP_UNKNOWN,
        FOSSIL_RESP_ERR,
        FOSSIL_RESP_OK,
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

typedef struct fossil_use_request
{
    fossil_request_t base;
    char *db_name;
} fossil_use_request_t;

typedef struct fossil_append_request
{
    fossil_request_t base;
    uint32_t len;
    const char *topic;
    const char *data;
} fossil_append_request_t;

typedef struct fossil_version_response
{
    fossil_response_t base;
    char *version;
} fossil_version_response_t;

fossil_message_t fossil_request_marshal(fossil_request_t *req)
{
    fossil_message_t message;

    memset(message.command, 0, 8);

    size_t len;
    uint32_t field_len = 0;
    fossil_version_request_t *version_req;
    fossil_use_request_t *use_req;
    fossil_append_request_t *append_req;

    switch (req->type)
    {
        case FOSSIL_REQ_VERSION:
            version_req = (fossil_version_request_t *)req;
            strcpy(message.command, "VERSION");
            len = strlen(version_req->version);
            message.data = malloc(len);
            memcpy(message.data, version_req->version, len);
            message.len = len;
            break;
        case FOSSIL_REQ_USE:
            use_req = (fossil_use_request_t *)req;
            strcpy(message.command, "USE");
            len = strlen(use_req->db_name);
            message.data = malloc(len);
            memcpy(message.data, use_req->db_name, len);
            message.len = len;
            break;
        case FOSSIL_REQ_APPEND:
            append_req = (fossil_append_request_t *)req;
            strcpy(message.command, "APPEND");
            len = strlen(append_req->topic) + strlen(append_req->data) + 4;
            field_len = strlen(append_req->topic);
            message.data = malloc(len);
            memcpy(message.data, &field_len, 4);
            memcpy(message.data + 4, append_req->topic, field_len);
            memcpy(message.data + 4 + field_len, append_req->data, append_req->len);
            message.len = len;
            break;

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

    if (strcmp(message->command, "OK") == 0)
    {
        fossil_response_t *ok_response = malloc(sizeof(fossil_response_t));
        ok_response->type = FOSSIL_RESP_OK;
        ok_response->code = *((uint32_t *)message->data);
        ok_response->explanation = malloc(message->len - 4);
        strcpy(ok_response->explanation, (char *)message->data + 4);
        return ok_response;
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
    ssize_t result;
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

fossil_response_t *fossil_send(fossil_client_t *client, fossil_request_t *request)
{
    fossil_response_t *response = NULL;
    fossil_message_t message, server_response = { .data = NULL};

    message = fossil_request_marshal(request);
    if (fossil_write_message(client, &message) < 0)
        goto cleanup;


    fossil_read_message(client, &server_response);
    response = fossil_response_unmarshal(&server_response);

cleanup:
    fossil_message_free(&message);
    fossil_message_free(&server_response);

    return response;
}

fossil_response_t *fossil_append(fossil_client_t *client, const char *topic, const char *data, uint32_t len)
{
    fossil_append_request_t request = { .base.type = FOSSIL_REQ_APPEND, .topic = topic, .data = data, .len = len};
    return fossil_send(client, (fossil_request_t *)&request);
}

ssize_t fossil_connect(fossil_client_t *client)
{
    ssize_t result = 0;

    if ((client->socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }

    client->addr.sin_family = AF_INET;
    // FIXME: Generalize port
    client->addr.sin_port = htons(8001);

    // FIXME: Generalize IP address
    if (inet_pton(AF_INET, "127.0.0.1", &client->addr.sin_addr) <= 0)
    {
        return -1;
    }

    if ((client->fd = connect(client->socket_fd, (struct sockaddr*)&client->addr, sizeof(client->addr))) < 0)
    {
        return -1;
    }

    // Now that we're connected, send a version advertisement
    fossil_version_request_t advertisement = {.base.type = FOSSIL_REQ_VERSION};
    advertisement.version = PROTOCOL_VERSION;

    fossil_version_response_t *server_version = (fossil_version_response_t *)fossil_send(client,(fossil_request_t *) &advertisement);

    if (server_version == NULL)
        goto cleanup;

    if (server_version->base.type == FOSSIL_RESP_ERR)
    {
        result = -1;
        goto cleanup;
    }

    // Now use the default database
    // FIXME: This should be configurable.
    fossil_use_request_t use_req = {.base.type = FOSSIL_REQ_USE, .db_name = "default"};
    fossil_response_t *use_resp = fossil_send(client, (fossil_request_t *)&use_req);

    if (use_resp->type != FOSSIL_RESP_OK)
    {
        result = -1;
        goto cleanup;
    }

cleanup:
    return result;
}

#endif //CFOSSIL_FOSSIL_H

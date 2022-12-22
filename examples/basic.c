#include <stdio.h>
#include "../fossil.h"

int main(int argc, char **argv) {
    fossil_client_t client;

    int result = fossil_connect(&client);
    if (result < 0) {
        return 1;
    }

    const char *data = "Sent Some Data From C!";
    fossil_response_t *response = fossil_append(&client, "/", data, strlen(data));
    if (response->type != FOSSIL_RESP_OK) {
        printf("%d %s", response->code, response->explanation);
    }

    return 0;
}
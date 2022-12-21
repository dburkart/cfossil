#include <stdio.h>
#include "fossil.h"

int main(int argc, char **argv) {
    fossil_client_t client;

    int result = fossil_connect(&client);

    return 0;
}
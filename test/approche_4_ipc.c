#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include "interfaceVL6180xmod.h"
#include "mainmodified.h"

#define SOCKET_PATH "/tmp/vl6180_socket"

int main() {
    struct sockaddr_un addr;
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Erreur de création du socket");
        return -1;
    }

    unlink(SOCKET_PATH);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Erreur de liaison du socket");
        close(sock_fd);
        return -1;
    }

    listen(sock_fd, 1);
    printf("En attente de connexion...\n");

    int client_fd = accept(sock_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("Erreur d'acceptation de connexion");
        close(sock_fd);
        return -1;
    }

    uint8_t fdPortI2C = open("/dev/i2c-1", O_RDWR);
    interfaceVL6810x_initialise(fdPortI2C);

    while (1) {
        float distance;
        if (interfaceVL6180x_litUneDistance(&distance) == 0) {
            write(client_fd, &distance, sizeof(distance));
            printf("Distance envoyée : %.2f mm\n", distance);
        }
        sleep(1);
    }

    close(client_fd);
    close(sock_fd);
    return 0;
}


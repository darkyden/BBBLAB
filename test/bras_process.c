#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include "uarm_swift_pro.h"

#define SOCKET_PATH "/tmp/vl6180_socket"

int main() {
    struct sockaddr_un addr;
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Erreur de création du socket");
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Erreur de connexion au socket");
        close(sock_fd);
        return -1;
    }

    int serial_port = init_serial("/dev/ttyACM0", B115200);

    while (1) {
        float distance;
        if (read(sock_fd, &distance, sizeof(distance)) > 0) {
            char command[50];
            snprintf(command, sizeof(command), "G0 X%.2f Y100 Z100 F5000", distance);
            send_gcode(serial_port, command);
            printf("Commande envoyée pour distance : %.2f mm\n", distance);
        }
    }

    close(serial_port);
    close(sock_fd);
    return 0;
}


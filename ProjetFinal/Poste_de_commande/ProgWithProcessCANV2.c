#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <errno.h>

#define CAN_ID 0x662  // Remplacez avec les 3 derniers chiffres de votre DA
#define CAN_INTERFACE "can0"
#define BUFFER_SIZE 8

void send_can_message(int socket_fd, int can_id, const char *message) {
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = can_id;
    frame.can_dlc = strlen(message);
    strncpy((char *)frame.data, message, frame.can_dlc);

    if (write(socket_fd, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("Erreur d'envoi du message CAN");
    } else {
        printf("Message envoyé : %s\n", message);
    }
}

void receive_can_message(int socket_fd, int pipe_fd) {
    struct can_frame frame;
    int nbytes;

    while (1) {
        nbytes = read(socket_fd, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            perror("Erreur de réception du message CAN");
            continue;
        }
        if (nbytes == sizeof(struct can_frame)) {
            printf("Message reçu dans le fils : ");
            for (int i = 0; i < frame.can_dlc; i++) {
                printf("%02X ", frame.data[i]);
            }
            printf("\n");

            // Envoie les données reçues au processus père
            write(pipe_fd, frame.data, frame.can_dlc);
        }
    }
}

int setup_can_socket(const char *interface) {
    int fdSocketCAN;
    struct sockaddr_can addr;
    struct ifreq ifr;

    if ((fdSocketCAN = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Erreur de création du socket CAN");
        return -1;
    }

    strcpy(ifr.ifr_name, interface);
    ioctl(fdSocketCAN, SIOCGIFINDEX, &ifr);

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fdSocketCAN, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur de liaison du socket CAN");
        return -1;
    }

    struct can_filter rfilter[1];
    rfilter[0].can_id = 0x903;
    rfilter[0].can_mask = 0xFFF;
    rfilter[1].can_id = 0x693;
    rfilter[1].can_mask = 0xFFF;

    setsockopt(fdSocketCAN, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    return fdSocketCAN;
}

int main() {
    int pid, socket_fd;
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Erreur de création du tube");
        return 1;
    }

    socket_fd = setup_can_socket(CAN_INTERFACE);
    if (socket_fd < 0) return 1;

    pid = fork();
    if (pid < 0) {
        perror("Erreur de création du processus fils");
        return 1;
    } else if (pid == 0) {  // Processus fils
        close(pipe_fd[0]);
        receive_can_message(socket_fd, pipe_fd[1]);
        close(pipe_fd[1]);
        close(socket_fd);
    } else {  // Processus père
        close(pipe_fd[1]);
        int choix;
        char message[BUFFER_SIZE];
        char received_message[BUFFER_SIZE];
        fd_set read_fds;
        struct timeval timeout;

        while (1) {
            // Affichage du menu
            printf("\nMenu :\n");
            printf("1. Envoyer un message CAN\n");
            printf("2. Quitter\n");
            printf("Votre choix : ");
            fflush(stdout);

            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(pipe_fd[0], &read_fds);
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int select_result = select(pipe_fd[0] + 1, &read_fds, NULL, NULL, &timeout);

            // Vérifie si le choix de l'utilisateur est disponible
            if (select_result > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
                scanf("%d", &choix);

                if (choix == 1) {
                    printf("Entrez le message (max 8 caractères) : ");
                    scanf("%s", message);
                    send_can_message(socket_fd, CAN_ID, message);
                } else if (choix == 2) {
                    kill(pid, SIGKILL);
                    break;
                } else {
                    printf("Choix invalide, veuillez réessayer.\n");
                }
            }

            // Vérifie si un message est reçu via le tube depuis le fils
            if (select_result > 0 && FD_ISSET(pipe_fd[0], &read_fds)) {
                ssize_t nbytes = read(pipe_fd[0], received_message, BUFFER_SIZE);
                if (nbytes > 0) {
                    received_message[nbytes] = '\0';
                    printf("Message reçu du fils dans le père : %s\n", received_message);
                }
            }
        }

        wait(NULL);
        close(socket_fd);
    }

    return 0;
}

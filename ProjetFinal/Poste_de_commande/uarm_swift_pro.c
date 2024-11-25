
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>

// Fonction pour initialiser le port série
int init_serial(const char *port_name, int baud_rate) {
    int serial_port = open(port_name, O_RDWR | O_NOCTTY);

    if (serial_port < 0) {
        perror("Erreur d'ouverture du port série");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(serial_port, &tty) != 0) {
        perror("Erreur lors de la configuration du port série");
        close(serial_port);
        return -1;
    }

    cfsetispeed(&tty, baud_rate);
    cfsetospeed(&tty, baud_rate);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8 bits de données
    tty.c_iflag &= ~IGNBRK;                     // Pas de contrôle du break
    tty.c_lflag = 0;                            // Pas de mode canonique
    tty.c_oflag = 0;                            // Pas de traitement de sortie
    tty.c_cc[VMIN] = 1;                         // Lecture bloquante
    tty.c_cc[VTIME] = 1;                        // Timeout en décisecondes

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);     // Pas de contrôle de flux logiciel
    tty.c_cflag |= (CLOCAL | CREAD);           // Activer le port série
    tty.c_cflag &= ~(PARENB | PARODD);         // Pas de parité
    tty.c_cflag &= ~CSTOPB;                    // 1 bit d'arrêt
    tty.c_cflag &= ~CRTSCTS;                   // Pas de contrôle de flux matériel

    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        perror("Erreur lors de l'application des paramètres du port série");
        close(serial_port);
        return -1;
    }

    return serial_port;
}

// Fonction pour lire une réponse du port série
void read_response(int serial_port) {
    char buffer[256];
    int n = read(serial_port, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Réponse reçue : %s\n", buffer);
    }
}

// Fonction pour envoyer une commande G-Code
void send_gcode(int serial_port, const char *command) {
    write(serial_port, command, strlen(command));
    write(serial_port, "\r", 1); // Ajout d'un retour chariot
    printf("Commande envoyée : %s\n", command);
    usleep(1000000); // Attendre pour éviter d'inonder le port
}

// Fonction pour effectuer les tests
void test_phase(int serial_port) {
    printf("Phase de test activée. Envoi des commandes de test...\n");

    send_gcode(serial_port, "M17"); // Activer les moteurs
    read_response(serial_port);

    send_gcode(serial_port, "G0 X100 Y100 Z100 F5000"); // Déplacer à une position test
    read_response(serial_port);

    send_gcode(serial_port, "G0 X200 Y50 Z150 F5000"); // Déplacer à une autre position test
    read_response(serial_port);

    send_gcode(serial_port, "G0 X0 Y0 Z0 F5000"); // Désactiver les moteurs
    read_response(serial_port);

    printf("Phase de test terminée.\n");
}

int main() {
    const char *serial_port_name = "/dev/ttyACM0"; // Port série
    int baud_rate = B115200; // Vitesse en bauds (à adapter si nécessaire)

    // Initialisation du port série
    int serial_port = init_serial(serial_port_name, baud_rate);
    if (serial_port < 0) {
        return EXIT_FAILURE;
    }

    printf("Connexion établie avec le uArm Swift Pro.\n");

    char input;
    printf("Appuyez sur 't' pour lancer la phase de test ou une autre touche pour continuer.\n");
    input = getchar();

    if (tolower(input) == 't') {
        test_phase(serial_port);
    }

    // Envoi des commandes principales
 //   send_gcode(serial_port, "M17");  // Activer les moteurs
   // read_response(serial_port);
//
  //  send_gcode(serial_port, "G0 X150 Y100 Z150 F10000"); // Déplacer à une position donnée
    //read_response(serial_port);

   // send_gcode(serial_port, "G0 X250 Y100 Z100 F5000"); // Déplacer à une autre position
    //read_response(serial_port);

    // Fermer le port série
    close(serial_port);

    printf("Programme terminé.\n");
    return EXIT_SUCCESS;
}

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "interfaceVL6180xmod.h"
#include "uarm_swift_pro.h"

// Variables globales pour le partage des données
pthread_mutex_t mutex_distance;
float distance_mesuree = 0.0;

// Thread pour lire les données du capteur
void *thread_capteur(void *arg) {
    uint8_t fdPortI2C = *((uint8_t *)arg);

    while (1) {
        float distance;
        if (interfaceVL6180x_litUneDistance(&distance) == 0) {
            pthread_mutex_lock(&mutex_distance);
            distance_mesuree = distance;
            pthread_mutex_unlock(&mutex_distance);
            printf("Capteur: Distance mesurée : %.2f mm\n", distance);
        } else {
            printf("Capteur: Erreur lors de la lecture de la distance.\n");
        }
        usleep(100000); // Pause courte entre les lectures pour une meilleure réactivité
    }

    return NULL;
}

// Thread pour contrôler le bras robotique
void *thread_bras(void *arg) {
    int serial_port = *((int *)arg);

    const char *positions[] = {
        "G0 X210 Y-50 Z120 F5000",
        "G0 X210 Y50 Z120 F5000",
        "G0 X270 Y50 Z120 F5000",
        "G0 X270 Y-50 Z120 F5000"
    };

    int num_positions = sizeof(positions) / sizeof(positions[0]);

    while (1) {
        for (int i = 0; i < num_positions; i++) {
            // Déplacer le bras vers la position spécifiée
            send_gcode(serial_port, positions[i]);
            printf("Bras: Commande envoyée : %s\n", positions[i]);

            usleep(2000000); // Attente pour que le bras atteigne la position

            // Vérifier si un objet est détecté
            pthread_mutex_lock(&mutex_distance);
            float distance = distance_mesuree;
            pthread_mutex_unlock(&mutex_distance);

            if (distance > 6 && distance < 17) { // Si un objet est détecté à moins de 50 mm
                char command[50];
                snprintf(command, sizeof(command), "G0 X210 Y-50 Z110 F5000"); // Descendre sur l'objet
                send_gcode(serial_port, command);
                printf("Bras: Objet détecté, descente sur l'objet à Z110\n");
                usleep(2000000); // Attente pour la descente
                break; // Arrêter la boucle sur détection
            }
        }
    }

    return NULL;
}

int main() {
    pthread_t thread1, thread2;

    // Initialisation du capteur VL6180x
    uint8_t fdPortI2C = open("/dev/i2c-1", O_RDWR);
    if (fdPortI2C == -1) {
        perror("Erreur: Impossible d'ouvrir le port I2C");
        return EXIT_FAILURE;
    }

    if (interfaceVL6810x_initialise(fdPortI2C) < 0) {
        perror("Erreur: Initialisation du capteur VL6180x échouée");
        close(fdPortI2C);
        return EXIT_FAILURE;
    }

    // Initialisation du bras uArm Swift Pro
    int serial_port = init_serial("/dev/ttyACM0", B115200);
    if (serial_port < 0) {
	serial_port = init_serial("/dev/ttyACM1", B115200);
	if (serial_port < 0){
           perror("Erreur: Impossible d'ouvrir le port série test.c");
           close(fdPortI2C);
           return EXIT_FAILURE;
	}
    }

    // Initialisation du mutex
    if (pthread_mutex_init(&mutex_distance, NULL) != 0) {
        perror("Erreur: Initialisation du mutex échouée");
        close(fdPortI2C);
        close(serial_port);
        return EXIT_FAILURE;
    }

    // Création des threads
    if (pthread_create(&thread1, NULL, thread_capteur, &fdPortI2C) != 0) {
        perror("Erreur: Impossible de créer le thread capteur");
        close(fdPortI2C);
        close(serial_port);
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread2, NULL, thread_bras, &serial_port) != 0) {
        perror("Erreur: Impossible de créer le thread bras");
        pthread_cancel(thread1);
        close(fdPortI2C);
        close(serial_port);
        return EXIT_FAILURE;
    }

    // Attente de la fin des threads (ne devrait pas arriver dans ce cas d'usage)
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // Nettoyage
    pthread_mutex_destroy(&mutex_distance);
    close(fdPortI2C);
    close(serial_port);

    return EXIT_SUCCESS;
}

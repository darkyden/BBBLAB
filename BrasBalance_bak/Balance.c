#include "Balance.h"
/**
 * @file    fork-uart.c
 * 
 * @brief Serial Port Programming in C
 * Non Cannonical mode 
 * Sellecting the Serial port Number on Linux   
 * /dev/ttyUSBx - when using USB  to Serial Converter, where x can be 0,1,2...etc  
 * /dev/ttySx   - for PC hardware based Serial ports, where x can be 0,1,2...etc 
 * termios structure -  /usr/include/asm-generic/termbits.h  
 * use "man termios" to get more info about  termios structure 
 * @author  Alexandre Dionne
 * @date    2024-10-07
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>   // File Control Definitions
#include <termios.h> // POSIX Terminal Control Definitions
#include <unistd.h>  // UNIX Standard Definitions
#include <errno.h>   // ERROR Number Definitions
#include <stdlib.h>
#include <sys/wait.h> 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>

// device port série à utiliser 
#define USB_BALANCE_PATH "dev/serial/by-id/ usb-FTDI_USB__-__Serial-if00-port0"
#define CAN_INTERFACE "can0"  // interface CAN

const char *portTTY = USB_BALANCE_PATH;

    struct can_frame frame;
int can_socket;
int fdp; // File Descriptor

unsigned char ucPoid;

void Initialise_CAN(void) {
    struct sockaddr_can addr;
    struct ifreq ifr;
    
    // Ouvrir un socket CAN
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket < 0) {
        perror("Erreur lors de l'ouverture du socket CAN");
        exit(EXIT_FAILURE);
    }

    // Associer le socket à l'interface CAN
    strcpy(ifr.ifr_name, "can0");
    ioctl(can_socket, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur lors de l'association du socket CAN");
        close(can_socket);
        exit(EXIT_FAILURE);
    }

    printf("Initialisation CAN terminée.\n");
}

void Initialise_PortUART(void)
{
    // Ouverture du port série
    fdp = open(portTTY, O_RDWR | O_NOCTTY);
    if (fdp == -1) {
        perror("Erreur lors de l'ouverture du port série");
        exit(EXIT_FAILURE);
    }

    // Configuration du port série
    struct termios SerialPortSettings;
    tcgetattr(fdp, &SerialPortSettings); // Récupérer les attributs actuels du port

    // Configurer la vitesse en bauds
    cfsetispeed(&SerialPortSettings, B9600);
    cfsetospeed(&SerialPortSettings, B9600);

    // Configurer 8N1 (8 bits de données, pas de parité, 1 bit d'arrêt)
    SerialPortSettings.c_cflag &= ~PARENB;
    SerialPortSettings.c_cflag &= ~CSTOPB;
    SerialPortSettings.c_cflag &= ~CSIZE;
    SerialPortSettings.c_cflag |= CS8;

    // Désactiver le contrôle de flux matériel
    SerialPortSettings.c_cflag &= ~CRTSCTS;
    SerialPortSettings.c_cflag |= CREAD | CLOCAL; // Activer la lecture et ignorer les lignes de contrôle du modem

    // Désactiver le contrôle de flux logiciel
    SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Mode non-canonique, sans écho ni signaux
    SerialPortSettings.c_lflag &= ~(ECHO | ECHOE | ISIG);
    SerialPortSettings.c_lflag |= ICANON;

    // Pas de traitement de sortie
    SerialPortSettings.c_oflag &= ~OPOST;

    // Configurer VMIN et VTIME pour une lecture d'au moins 1 caractère avec un délai infini
    SerialPortSettings.c_cc[VMIN] = 1;
    SerialPortSettings.c_cc[VTIME] = 0;

    // Appliquer les paramètres
    if (tcsetattr(fdp, TCSANOW, &SerialPortSettings) != 0) {
        perror("Erreur lors de la configuration des attributs du port série");
        exit(EXIT_FAILURE);
    }

    tcflush(fdp, TCIFLUSH); // Vider le tampon d'entrée du port série
}

void _main (void)
{
    printf("Début du programme\n");

	Initialise_PortUART();
 	 Initialise_CAN();       // Initialiser le CAN

    //LirePortSerie();

    LirePoids();
	printf("Erreur, veuillez redémmarer\n");
}

//void LirePortSerie(void)
  void LirePoids(void)
{
    char read_buffer[64]; // Tampon pour stocker les données reçues
    int nbytes;
    
    while (1) {
        // Lecture des données du port série
            nbytes = read(fdp, read_buffer, sizeof(read_buffer));
        if (nbytes < 0) {
            perror("Erreur de lecture");
            exit(EXIT_FAILURE);
        }

        read_buffer[nbytes] = '\0'; // Ajouter le caractère de fin de chaîne
        if(read_buffer[0] == 'G')
        {
            ucPoid = (10*(read_buffer[9] - 0x30)) ;
            ucPoid = (ucPoid + (read_buffer[10] - 0x30));   
            printf("%u \n",ucPoid);
        }
        
            // Préparer le message CAN
            frame.can_id = 0x031; // Identifiant du message CAN
            frame.can_dlc = 1;    // Taille des données (1 octet)
            frame.data[0] = ucPoid;

            // Envoyer le message CAN
            if (write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
                perror("Erreur lors de l'envoi du message CAN");
            } else {
                printf("Message CAN envoyé : Poids = %u\n", ucPoid);
            }

    }
    close(fdp); // Fermer le port série
}




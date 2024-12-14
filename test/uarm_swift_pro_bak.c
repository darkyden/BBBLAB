
#include "uarm_swift_pro.h"

#define MOVING 1
#define STOP 0
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
char* read_response(int serial_port) {
    //char buffer[256];
    char* buffer = malloc(256);
    if(buffer == NULL){ return NULL; } // Gestion de l'erreur d'allocation

    int n = read(serial_port, buffer, 255); 
    
    if (n > 0) {
        buffer[n] = '\0'; // Ajout d'un terminateur de chaine
        printf("Réponse reçue : %s\n", buffer);
        return buffer; // Retourne le buffer
    }

    free(buffer); // Libère la mémoire en cas d'erreur
    return NULL; // Retourne NULL en cas d'erreur ou de lecture vide
}

int arm_still_moving(int serial_port){
printf("Still moving ????? \n");
   send_gcode(serial_port,"M2200");
   
   char* moving = read_response(serial_port);
   char v_value;

   if(moving != NULL){
	   if(sscanf(moving,"ok V%d",&v_value) == STOP){
		   printf("le Bras s'est arrété !!!\n");
		   return STOP;
	   }else{
		   printf("le Bras ne s'est pas arrété !!!\n");
		   return MOVING;
  	   }
   }

   return -1;
}

// Fonction pour obtenir les coordonnées actuelles du bras
void get_current_coordonnates(int serial_port, int *x_rounded, int *y_rounded, int *z_rounded){
    
printf("get_current_coordonnates !!!\n"); 
	send_gcode(serial_port, "P2220");
	//send_gcode(serial_port, "M114");
///	usleep(2000000);
	char* response = read_response(serial_port);
	float x_response,y_response,z_response;


	if(response != NULL){
	   if(sscanf(response,"ok X%f Y%f Z%f",&x_response,&y_response,&z_response)==3){
                *x_rounded = (int)round(x_response);
                *y_rounded = (int)round(y_response);
                *z_rounded = (int)round(z_response);

                printf("Réponse reçue : %s\n", response);
                printf("x_response : %f\n", x_response);
                printf("X_rounded : %d\n", x_rounded);
                printf("Y_response : %f\n", y_response);
                printf("Y_rounded : %d\n", y_rounded);
                printf("Z_response : %f\n", z_response);
                printf("Z_rounded : %d\n", z_rounded);
           }

	   free(response); // Libération de la mémoire
	
	}else{
		printf("Erreur lors de la lecture de la réponse.\n");
	}
}


// Fonction pour envoyer une commande G-Code
void send_gcode(int serial_port, const char *command) {
    write(serial_port, command, strlen(command));
    write(serial_port, "\r", 1); // Ajout d'un retour chariot
    printf("Commande envoyée : %s\n", command);
    usleep(50000); // Attendre pour éviter d'inonder le port
}

// Fonction pour effectuer les tests
void test_phase(int serial_port) {
    printf("Phase de test activée. Envoi des commandes de test...\n");

    send_gcode(serial_port, "M17"); // Activer les moteurs
    read_response(serial_port);
    usleep(1000000); // Attendre pour éviter d'inonder le port

    send_gcode(serial_port, "G0 X100 Y100 Z100 F200000"); // Déplacer à une position test
    read_response(serial_port);
    while(arm_still_moving(serial_port) == MOVING){
    usleep(1000000); // Attendre pour éviter d'inonder le port
    }
    send_gcode(serial_port, "G0 X200 Y50 Z150 F200000"); // Déplacer à une autre position test
    read_response(serial_port);
    while(arm_still_moving(serial_port) == MOVING){
    usleep(1000000); // Attendre pour éviter d'inonder le port
    }
    send_gcode(serial_port, "G0 X0 Y0 Z0 F200000"); // Désactiver les moteurs
    read_response(serial_port);
    while(arm_still_moving(serial_port) == MOVING){
    usleep(1000000); // Attendre pour éviter d'inonder le port
    }
    printf("Phase de test terminée.\n");
}

int main_() {
    const char *serial_port_name = "/dev/ttyACM0"; // Port série
    int baud_rate = B115200; // Vitesse en bauds (à adapter si nécessaire)

    // Initialisation du port série
    int serial_port = init_serial(serial_port_name, baud_rate);
    if (serial_port < 0) {
	    serial_port = init_serial("/dev/ttyACM1",baud_rate);
	    if(serial_port < 0){
                return EXIT_FAILURE;
	    }
    }

    printf("Connexion établie avec le uArm Swift Pro.\n");

    char input;
    printf("Appuyez sur 't' pour lancer la phase de test ou une autre touche pour continuer.\n");
    input = getchar();

    if (tolower(input) == 't') {
        test_phase(serial_port);
    }


    // Fermer le port série
    close(serial_port);

    printf("Programme terminé.\n");
   return EXIT_SUCCESS;
}

//int main(){
//	return main_();
//	
//}

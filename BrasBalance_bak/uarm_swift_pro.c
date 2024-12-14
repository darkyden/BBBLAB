
#include "uarm_swift_pro.h"

#define USB_UARM_PATH "/dev/serial/by-id/usb-Arduino__www.arduino.cc__Arduino_Mega_2560_75735323330351800291-if00"


#define MOVING 1
#define STOP 0
#define GRAB 2
#define NOGRAB 0
#define TRIGGERED 1
#define NOTRIGGERED 0
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
    tty.c_lflag = ICANON;                            // Pas de mode canonique
    tty.c_oflag = 0;                            // Pas de traitement de sortie
//    tty.c_cc[VMIN] = 1;                         // Lecture bloquante
//    tty.c_cc[VTIME] = 1;                        // Timeout en décisecondes

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

//	tcflush(serial_port, TCIFLUSH); // Purge des données d'entrée avant lecture
    char* buffer = malloc(256);
    if(buffer == NULL){ return NULL; } // Gestion de l'erreur d'allocation

    int n = read(serial_port, buffer, 255); 
    
    if (n > 0) {
        buffer[n] = '\0'; // Ajout d'un terminateur de chaine
        printf("Réponse reçue : %s\n", buffer);
	fflush(stdin);
        return buffer; // Retourne le buffer
    }

    free(buffer); // Libère la mémoire en cas d'erreur
    return NULL; // Retourne NULL en cas d'erreur ou de lecture vide
}

// Fonction pour vérifier l'état du limiteur
int limit_switch(int serial_port) {
    // Envoyer la commande au besoin (décommenter si nécessaire)
     send_gcode(serial_port, "P2233");

    // Lire la réponse
    char* triggered = read_response(serial_port);

    // Vérifier si une réponse a été reçue
    if (triggered != NULL) {
        int v_triggered; // Variable pour capturer la valeur numérique de "V"

        // Analyser la réponse pour vérifier si elle correspond au format attendu
        //if (sscanf(triggered, "@6 N0 V%d", &v_triggered) == 1 && v_triggered == TRIGGERED) {
        if (sscanf(triggered, "ok V%d", &v_triggered) == 1 && v_triggered == TRIGGERED) {
            printf("Objet en contact !!!\n");
            free(triggered); // Libérer la mémoire
            return TRIGGERED;
       }

            printf("Pas Objet en contact !!!\n");
        // Libérer la mémoire même si la réponse est invalide
        free(triggered);
    }

    // Si aucune réponse valide n'a été reçue, retourner NOTRIGGERED
    return NOTRIGGERED;
}


// Fonction pour vérifier si un objet a été pris

int grab_object(int serial_port) {
    
	// Envoyer la commande pour activer la pompe
    	send_gcode(serial_port, "P2231");

    	// Lire la réponse
    	char* grabbing = read_response(serial_port);
    
	// Vérifier si une réponse a été reçue
	if (grabbing != NULL) {
        
		int v_grabbing; // Variable pour capturer la valeur numérique de "V"

        	// Vérifier si la réponse correspond au format attendu
        
		if (sscanf(grabbing, "ok V%d", &v_grabbing) == 1 && v_grabbing == GRAB) {
            
			printf("La pompe a attrapé un objet !\n");
            		free(grabbing); // Libérer la mémoire du buffer
            		return GRAB;
        		}else{
				switch(v_grabbing){
					case 0 :	
						printf("Pompe arrêtée.\n");
						// Libérer la mémoire même si la réponse est invalide
						free(grabbing);				
						return NOGRAB; // Pompe arrêtée
					case 1:
						printf("Pompe en marche, mais rien n'a été attrapé.\n");
						// Libérer la mémoire même si la réponse est invalide
						free(grabbing);
						return NOGRAB; // Pompe active mais rien attrapé
					default:
						printf("Réponse inattendue : ok V%d\n", v_grabbing);
						// Libérer la mémoire même si la réponse est invalide
						free(grabbing);
						// return NOGRAB; // Réponse inattendue
				}
			}
		// Si la réponse est invalide ou non reçue, retourner NOGRAB
		return NOGRAB;
	}else{
        	printf("Aucune réponse reçue de la pompe.\n");
	}

    // Retourner NOGRAB si la réponse est invalide ou non reçue
    return NOGRAB;
}



//int grab_object(int serial_port){

//	send_gcode(serial_port, "#25 P2231");
//	char* grabbing = read_response(serial_port);
//	char v_grabbing;

//	if(grabbing !=NULL){
//	   if(sscanf(grabbing, "$25 ok V%d",&v_grabbing) == GRAB){
//	        printf("la pump a attrappé de quoi !!!\n");
//		return GRAB;
//	   }
//	}

//	return NOGRAB;

//}

void delay(int serial_port,int idelay){
     
    char commandelay[6];
    snprintf(commandelay, sizeof(commandelay), "G2004 P%d",idelay);
   usleep(2000000); 
   char* response;
   while (1) {
    response = read_response(serial_port); // Lire la réponse du port série
    if (response == NULL) {
        printf("Erreur de lecture ou de mémoire.\n");
        continue; // Recommencer la boucle si une erreur s'est produite
    }

    if (strcmp(response, "ok") == 0) {
        printf("Réponse OK reçue : %s\n", response);
        free(response); // Libère la mémoire allouée par read_response
        break; // Quitte la boucle
    } else {
        printf("Réponse inattendue : %s\n", response);
        free(response); // Libère la mémoire allouée
    }
  }
}

// Fonction pour vérifier si le bras est en mouvement
int arm_still_moving(int serial_port){

	printf("Still moving ????? \n");
   send_gcode(serial_port,"#25 M2200");
   
   char* moving = read_response(serial_port);
   char v_moving;

   if(moving != NULL){
	   if(sscanf(moving,"$25 ok V%d",&v_moving) == STOP){
		   printf("le Bras s'est arrété !!!\n");
		   return STOP;
	   }
	   
	   printf("le Bras ne s'est pas arrété !!!\n");
	   return MOVING;
   }

   return -1;
}

void pump_on(int serial_port)
{ 
	send_gcode(serial_port,"#25 M2231 V1");
	usleep(2000000);
}

void pump_off(int serial_port)
{ 
	send_gcode(serial_port,"#25 M2231 V0");
	usleep(2000000);
}

// Fonction pour obtenir les coordonnées actuelles du bras
void get_current_coordonnates(int serial_port, int *x_rounded, int *y_rounded, int *z_rounded){
    
    printf("get_current_coordonnates !!!\n"); 
	send_gcode(serial_port, "#25 P2220");
    usleep(1000000); // Attendre pour éviter d'inonder le port
	
	char* response = read_response(serial_port);
	float x_response,y_response,z_response;


	if(response != NULL){
	   if(sscanf(response,"$25 ok X%f Y%f Z%f",&x_response,&y_response,&z_response)==3){
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
	// Vider les tampons avant d'envoyer une commande
    tcflush(serial_port, TCOFLUSH);
    write(serial_port, command, strlen(command));
    write(serial_port, "\n", 1); // Ajout d'un retour chariot
    printf("Commande envoyée : %s\n", command);
    usleep(50000); // Attendre pour éviter d'inonder le port
}

// Fonction pour effectuer les tests
void test_phase(int serial_port) {
    printf("Phase de test activée. Envoi des commandes de test...\n");

    send_gcode(serial_port, "#25 M17"); // Activer les moteurs
    usleep(1000000); // Attendre pour éviter d'inonder le port
    read_response(serial_port);
    usleep(1000000); // Attendre pour éviter d'inonder le port

    send_gcode(serial_port, "#25 G0 X200 Y50 Z150 F15000"); // Déplacer à une position test
    usleep(1000000); // Attendre pour éviter d'inonder le port
    read_response(serial_port);
    usleep(1000000); // Attendre pour éviter d'inonder le port
    send_gcode(serial_port, "#25 G0 X200 Y-50 Z150 F15000"); // Déplacer à l'autre position test
    usleep(1000000); // Attendre pour éviter d'inonder le port
    read_response(serial_port);
    usleep(1000000); // Attendre pour éviter d'inonder le port
    send_gcode(serial_port, "#25 M18"); // Activer les moteurs
    usleep(1000000); // Attendre pour éviter d'inonder le port
    read_response(serial_port);
    usleep(100000); // Attendre pour éviter d'inonder le port

    printf("Phase de test terminée.\n");
}

int main_() {
    
    int baud_rate = B115200; // Vitesse en bauds (à adapter si nécessaire)

    // Initialisation du port série
    int serial_port = init_serial(USB_UARM_PATH, baud_rate);
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


    // Fermer le port série
    close(serial_port);

    printf("Programme terminé.\n");
   return EXIT_SUCCESS;
}


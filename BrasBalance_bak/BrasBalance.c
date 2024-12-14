#include "interfaceVL6180xmod.h"
#include "uarm_swift_pro.h"
#include "Balance.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdbool.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <errno.h>


#define USB_ARM_PATH "/dev/serial/by-id/usb-Arduino__www.arduino.cc__Arduino_Mega_2560_75735323330351800291-if00"
#define USB_BALANCE_PATH "dev/serial/by-id/ usb-FTDI_USB__-__Serial-if00-port0"
#define CAN_INTERFACE "can0"  // interface CAN
#define CAN_ID_INFO 0x034  // ID 

#define ORANGE 'O'

// Variables globales pour le partage des données
pthread_mutex_t mutex_distance;
pthread_cond_t cond_suspend = PTHREAD_COND_INITIALIZER; // Condition pour suspension/reprise
pthread_mutex_t mutex_suspend = PTHREAD_MUTEX_INITIALIZER; // Mutex pour la condition

pthread_mutex_t mutex_start = PTHREAD_MUTEX_INITIALIZER; // Mutex pour synchroniser le démarrage
pthread_cond_t cond_start = PTHREAD_COND_INITIALIZER;   // Condition pour le démarrage des threads

int start_threads = 0; // Flag indiquant si les threads peuvent démarrer

enum SystemState { WAITING, RUNNING, STOPPED };
volatile enum SystemState system_state = WAITING;


float distance_mesuree = 0.0;
volatile int stop_threads = 0; // Variable pour signaler l'arrêt
volatile int suspend_threads = 0;  // Suspension des threads
char BlockColor;

void set_terminal_mode(int enable) {
    static struct termios oldt, newt;

    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// Fonction de réception CAN (à adapter selon la bibliothèque CAN que vous utilisez)
void *thread_can_attente(void *arg) {
    int can_socket;
    struct can_frame frame;
    int nbytes;

    // Ouvrir le socket CAN (exemple avec libsocketcan)
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket < 0) {
        perror("Erreur d'ouverture du socket CAN");
        return NULL;
    }

    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = if_nametoindex(CAN_INTERFACE);

    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur de bind");
        close(can_socket);
        return NULL;
    }

    printf("Thread CAN: En attente du message ...\n");
    
    while (1) {
        
	    // Lire un message CAN
	    nbytes = read(can_socket, &frame, sizeof(struct can_frame));
        
	    if (nbytes > 0) {
            
		    // Si l'ID du message est celui de "Orange"
		    if (frame.can_id == CAN_ID_INFO) {
                
			    printf("Thread CAN: Message reçu. Lancement des threads...\n");
				
			    if(frame.data[1]==0x63 || frame.data[1] == 0x62){
			
				    if(frame.data[1] == 0x63){BlockColor = ORANGE;}
            		
				    // Déclencher les autres threads
					
				    pthread_mutex_lock(&mutex_start);
				    //start_threads = 1;
				    system_state = RUNNING; // Passer en mode "exécution"
				    pthread_cond_broadcast(&cond_start);  // Lancer tous les threads en attente
				    pthread_mutex_unlock(&mutex_start);
				
				    break;  // Quitter la boucle après avoir lancé les threads
			    }
		    }
	    }
        usleep(100000); // Pause pour éviter de surcharger le CPU
    }

    close(can_socket);
    return NULL;
}

void *thread_serie_attente(void *arg) {

	int serial_port = *((int *)arg);
    
	char buffer[64];
    
	memset(buffer, 0, sizeof(buffer)); // Initialiser le buffer
    
	int nbytes;
    
	printf("Thread série: En attente du message '0x63' ou '0x62'...\n");

    while (1) {
        // Lecture des données depuis le port série
        nbytes = read(serial_port, buffer, sizeof(buffer) - 1);
        if (nbytes > 0) {
            buffer[nbytes] = '\0'; // Terminer la chaîne
            printf("Thread série: Reçu: %s\n", buffer);

            // Vérifier si "Orange" ou "Metal" est reçu
            if (strstr(buffer, "0x63") || strstr(buffer, "0x62")) {
                printf("Thread série: Message 'Orange' reçu. Lancement des threads...\n");
				    
		if(strstr(buffer,"0x63")){BlockColor = ORANGE;}

                // Déclencher les autres threads
                pthread_mutex_lock(&mutex_start);
                system_state = RUNNING; // Passer en mode "exécution"
		//start_threads = 1; // Modifier le flag pour démarrer les threads
                pthread_cond_broadcast(&cond_start); // Libérer tous les threads en attente
                pthread_mutex_unlock(&mutex_start);

                break; // Quitter la boucle après avoir lancé les threads
            }
        }
        usleep(100000); // Pause pour éviter de surcharger le CPU
    }

    return NULL;
}



void *thread_clavier(void *arg) {
    set_terminal_mode(1); // Activer le mode non-bloquant
    
	// Attente du signal de démarrage
    pthread_mutex_lock(&mutex_start);
    while (!start_threads) {
        pthread_cond_wait(&cond_start, &mutex_start);
    }
    pthread_mutex_unlock(&mutex_start);
	
    while (!stop_threads) {
        char c = getchar();
        if (c == 'Q' || c == 'q') {
            printf("Touche 'Q' détectée. Arrêt du programme...\n");
            //stop_threads = 1; // Signaler aux autres threads qu'il faut s'arrêter
            system_state = STOPPED;
	    //pthread_cond_broadcast(&cond_suspend); // Libérer les threads suspendus pour qu'ils puissent sortir
	    pthread_cond_broadcast(&cond_start);
            pthread_mutex_unlock(&mutex_start);
	    break;
        } else if (c == 'S' || c == 's') {
            pthread_mutex_lock(&mutex_suspend);
            suspend_threads = !suspend_threads; // Alterner entre suspendre et reprendre
            if (!suspend_threads) {
                pthread_cond_broadcast(&cond_suspend); // Reprendre tous les threads suspendus
                printf("Reprise du programme...\n");
            } else {
                printf("Programme suspendu...\n");
            }
            pthread_mutex_unlock(&mutex_suspend);
        } else if (c == 'W' || c == 'w') { // Retour à l'état "attente"
            printf("Retour en mode attente.\n");
            pthread_mutex_lock(&mutex_start);
            system_state = WAITING;
            pthread_mutex_unlock(&mutex_start);
        }
    }

        usleep(100000); // Pause courte pour réduire la charge du CPU
    set_terminal_mode(0); // Restaurer le mode normal
    return NULL;
}


// Thread pour lire les données du capteur
void *thread_capteur(void *arg) {
    uint8_t fdPortI2C = *((uint8_t *)arg);
	
// Attente du signal de démarrage
    pthread_mutex_lock(&mutex_start);
   // while (!start_threads) {
     //   pthread_cond_wait(&cond_start, &mutex_start);
    //}
    while (system_state != RUNNING && !stop_threads) {
        pthread_cond_wait(&cond_start, &mutex_start); // Attente de passage à "RUNNING"
    }
    pthread_mutex_unlock(&mutex_start);
	
    while (1) {
        float distance;
        if (interfaceVL6180x_litUneDistance(&distance) == 0) {
            pthread_mutex_lock(&mutex_distance);
            distance_mesuree = distance;
            pthread_mutex_unlock(&mutex_distance);
           // printf("Capteur: Distance mesurée : %.2f mm\n", distance);
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
	// Attente du signal de démarrage
    pthread_mutex_lock(&mutex_start);
    //while (!start_threads) {
     //   pthread_cond_wait(&cond_start, &mutex_start);
    //}

    while (system_state != RUNNING && !stop_threads) {
        pthread_cond_wait(&cond_start, &mutex_start); // Attente de passage à "RUNNING"
    }
    pthread_mutex_unlock(&mutex_start);

    tcflush(serial_port, TCIOFLUSH);  // Vide les tampons d'entrée et de sorti

    // Vérifie si le thread doit être suspendu
        pthread_mutex_lock(&mutex_suspend);
        while (suspend_threads && !stop_threads) {
            pthread_cond_wait(&cond_suspend, &mutex_suspend); // Suspend le thread
        }
        pthread_mutex_unlock(&mutex_suspend);

  const char *positions[] = {
      	"#25 G0 X165 Y-30 Z120 F5000",
	"#25 G0 X230 Y-30 Z120 F5000",
	"#25 G0 X230 Y-10 Z120 F5000",
	"#25 G0 X165 Y-10 Z120 F5000",
	"#25 G0 X165 Y5 Z120 F5000",
	"#25 G0 X230 Y5 Z120 F5000",
  };
    
 
  const char *Pousse[] = {
	"#25 G0 X165 Y-15 Z120 F5000",
	"#25 G0 X165 Y-15 Z100 F5000",
	"#25 G0 X240 Y-15 Z100 F5000",
	"#25 G0 X240 Y-15 Z120 F5000",
	"#25 G0 X165 Y-10 Z120 F5000",
	"#25 G0 X165 Y-10 Z100 F5000",
	"#25 G0 X240 Y-10 Z100 F5000",
	"#25 G0 X240 Y-10 Z120 F5000"
  };

  const char *Balayage[] = {
	"#25 G0 X255 Y40 Z120",
	"#25 G0 X255 Y40 Z110",
	"#25 G0 X255 Y-10 Z110",
	"#25 G0 X255 Y-10 Z120"
  };

  const char *Levage[] = {
	"#25 G0 X260 Y-40 Z120",
	"#25 G0 X260 Y-40 Z105"
  };


  int thisProg = 2;

    int num_positions = 5;
    int object_found =0;
    int count = 1;
    bool bZigZag = false;      
    send_gcode(serial_port, positions[0]);
    usleep(5000000); 
    
    while (!stop_threads) {
     
	    tcflush(serial_port, TCIOFLUSH);  // Purge des tampons
	    
	    for (int i = 0; i <= num_positions && !stop_threads; i++) {
         
		    printf("Bras: Déplacement vers : %s et num_positions : %d \n", positions[i], i);
          
		    // Déplace le bras vers la position spécifiée
          
		    send_gcode(serial_port, positions[i]);
    
		    usleep(200000); 
             
		    tcflush(serial_port, TCIFLUSH);  // Purge des tampons d'entrée avant lecture
	  
		    // Boucle pour surveiller la détection pendant le déplacement
             
		    for (int t = 0; t < 50 && !stop_threads; t++) { // 50 x 10ms = 500ms
		
			    pthread_mutex_lock(&mutex_distance);
			    float distance = distance_mesuree;
			    pthread_mutex_unlock(&mutex_distance);
		
			    if (distance >= 0 && distance <= 6.50) {
		    
				    if (thisProg == 1){

					    float coord_x, coord_y, coord_z;
				
					    printf("Bras: Objet détecté à %.2f mm, arrêt du mouvement.\n", distance);
				    
					    if(i == 1 || i == 5) { i -= 1;} else if (i == 2){i += 1;} 
				    
					    send_gcode(serial_port,"#25 G2203");
					    usleep(2000000);
                   
					    send_gcode(serial_port, positions[i]);
					    usleep(2000000);
		
					    sscanf(positions[i], "#25 G0 X%f Y%f Z%f F5000", &coord_x, &coord_y, &coord_z);
				    
					    coord_x += 60;// Avance de 10 mm sur X
				    
					    tcflush(serial_port, TCIFLUSH);  // Purge des tampons d'entrée avant lecture
				    
					    // Génération de la commande ajustée
					    char command[100];
				    
					    while(1){
				    
						    pump_off(serial_port);
						    usleep(2000000);
					    
						    do{
							    printf("je check\n");
						    
							    bZigZag = !bZigZag;
						    
							    coord_z = 125; 
						    
							    snprintf(command, sizeof(command), "#25 G0 X%.2f Y%.2f Z%.2f F10000", coord_x, coord_y, coord_z);
							    // Descente pour manipuler l'objet
						    
							    send_gcode(serial_port, command); // Envoi de la commande ajustée
							    usleep(2000000);
						    
							    coord_z = 100; // Descend sur l'objet
					
							    if(bZigZag){
								    snprintf(command, sizeof(command), "#25 G0 X%.2f Y%.2f Z%.2f F10000", coord_x, coord_y - 5, coord_z);
							    }else{
								    snprintf(command, sizeof(command), "#25 G0 X%.2f Y%.2f Z%.2f F10000", coord_x, coord_y + 5, coord_z);
							    }
					
							    send_gcode(serial_port, command); // Envoi de la commande ajustée
							    usleep(2000000);
			
							    coord_x += 15;// Avance de 10 mm sur X
					
							    tcflush(serial_port, TCIFLUSH); // Purge des données d'entrée avant lecture
				    
						    }while(limit_switch(serial_port)==0);
					    
						    pump_on(serial_port);
						    //usleep(2000000);

						    tcflush(serial_port, TCIFLUSH); // Purge des données d'entrée avant lecture
					   
						    if(grab_object(serial_port) == 2){coord_z = 160 ; break;}
					    }
					    
				    
					    tcflush(serial_port, TCIFLUSH); // Purge des données d'entrée avant lecture
                   
					    snprintf(command, sizeof(command), "#25 G0 X%.2f Y%.2f Z%.2f F10000", coord_x, coord_y, coord_z);
			
					    send_gcode(serial_port, command); // Envoi de la commande ajustée
					    usleep(2000000);
				    
				    }else{
					    
					    printf("Bras: Objet détecté à %.2f mm, arrêt du mouvement.\n", distance);
                                        
					    send_gcode(serial_port,"#25 G2203");
					    usleep(2000000);
                                        
					    if(i <= 3){
                                            
						    for(int u = 0; u <= 3; u++){
							    send_gcode(serial_port, Pousse[u]);
							    usleep(2000000);
						    }
					    }else{
                                                
						    for(int u = 4; u <= 7; u++){
							    send_gcode(serial_port, Pousse[u]);
							    usleep(2000000);
						    }
					    }


					    for(int j = 0; j <= 3; j++){
						    send_gcode(serial_port, Balayage[j]);
						    usleep(2000000);
					    }

					    for(int q = 0; q <= 1; q++){
						    send_gcode(serial_port,Levage[q]);
						    usleep(2000000);
					    }
                                        
					    pump_on(serial_port);
					    usleep(2000000);
                                        
					    send_gcode(serial_port,Levage[0]);
					    usleep(2000000);
				    }


				    send_gcode(serial_port, "#25 G0 X165 Y-10 Z150 F10000");
				    usleep(2000000);
                    
				    send_gcode(serial_port, "#25 G0 X15 Y285 Z150 F10000");
				    usleep(2000000);
        
		            
				    if(BlockColor != ORANGE){
				    
					    send_gcode(serial_port, "#25 G0 X15 Y285 Z105 F10000");
				    
					    usleep(2000000);
				    
					    pump_off(serial_port);
				
					    usleep(2000000);
					
					    LirePoids();
				    
					    send_gcode(serial_port, "#25 G0 X15 Y285 Z150 F10000");
					    usleep(2000000);

					    send_gcode(serial_port, "#25 G0 X15 Y285 Z105 F10000");
					    usleep(2000000);
				    
					    pump_on(serial_port);
					    usleep(2000000);
				    }
					
				    send_gcode(serial_port, "#25 G0 X15 Y285 Z150 F10000");
				    usleep(2000000);
                    
				    send_gcode(serial_port, "#25 G0 X15 Y145 Z140 F10000");
				    usleep(2000000);
                    
				    send_gcode(serial_port, "#25 G0 X15 Y145 Z55 F10000");
				    usleep(2000000);
		    
				    pump_off(serial_port);
				    usleep(2000000);
                    
				    send_gcode(serial_port, positions[0]);
				    usleep(2000000);
		
		    // Attente pour la descente
//		   break; // Arrête la surveillance et le mouvement
			  
			    }
                
                //pthread_mutex_unlock(&mutex_distance);

                usleep(35000); // Pause de 10ms entre chaque vérification pour un osti bras de marde disfonctionnel
            }

            // Si un objet est détecté, sortir de la boucle des positions
              pthread_mutex_lock(&mutex_distance);
              if (distance_mesuree >= 0 && distance_mesuree <= 6.4) {
                
		      pthread_mutex_unlock(&mutex_distance);

                break;
	      }
            
            pthread_mutex_unlock(&mutex_distance);

            usleep(500000); // Pause courte pour assurer la fluidité
   
	    }

    }
}


int main() {

    pthread_t thread1, thread2, thread3, thread4;

	// Création du thread pour écouter les messages CAN
    if (pthread_create(&thread4, NULL, thread_can_attente, NULL) != 0) {
        perror("Erreur: Impossible de créer le thread CAN");
        return EXIT_FAILURE;
    }

	
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
    int serial_port = init_serial(USB_ARM_PATH, B115200);
    if (serial_port < 0) {
           perror("Erreur: Impossible d'ouvrir le port série test.c");
           close(fdPortI2C);
           return EXIT_FAILURE;
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

    if (pthread_create(&thread3, NULL, thread_clavier, NULL) != 0){
       perror("Erreur : Impossible de créer le thread clavier");
       pthread_cancel(thread2);
       close(fdPortI2C);
       close(serial_port);
       return EXIT_FAILURE;
    }

// Création du thread pour surveiller le port série
    if (pthread_create(&thread4, NULL, thread_serie_attente, &serial_port) != 0) {
        perror("Erreur: Impossible de créer le thread série pour 'Orange'");
        pthread_cancel(thread1);
        pthread_cancel(thread2);
        pthread_cancel(thread3);
        close(serial_port);
        return EXIT_FAILURE;
    }
    // Attente de la fin des threads (ne devrait pas arriver dans ce cas d'usage)
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
	    pthread_join(thread4, NULL);

    // Nettoyage
	pthread_mutex_destroy(&mutex_start);
    pthread_cond_destroy(&cond_start);
    pthread_mutex_destroy(&mutex_distance);
    pthread_mutex_destroy(&mutex_suspend);
    pthread_cond_destroy(&cond_suspend);
    close(fdPortI2C);
    close(serial_port);

    return EXIT_SUCCESS;
}


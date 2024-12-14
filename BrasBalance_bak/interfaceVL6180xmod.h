#ifndef INTERFACEVL6180XMOD_H
#define INTERFACEVL6180XMOD_H
#include "mainmodified.h"

//Description: interface qui permet de lire des distances à l'aide d'un VL6180x
//broches: 
//      vue de dessus
//       _____________
//      |             |
//      |             |
//      |   _  _  _   |
//      |  |_||_||_|  |
//      |             |
//      |scl sda  gnd |
//      |int gpio vdd |
//      |_____________|
//
//Note:
//Des pull-up de 4.7k sont requises sur scl et sda si celles du BbB n'y sont pas
//Une pull-up de 10k est requise pour gpio pour permettre au VL6180x de bien démarré
//Le vl6180x ne peut être utilisé pendant la première milliseconde qui suit sont alimentation.
//Il faut donc attendre ce temps avant de communiquer avec lui.

#define INTERFACEVL6180X_INFORMATION_TRAITEE 0
#define INTERFACEVL6180X_INFORMATION_DISPONIBLE 1
#define INTERFACEVL6180X_MODULE_EN_ARRET 0
#define INTERFACEVL6180X_MODULE_EN_FONCTION 1
extern volatile int flag_detM; // 0: pas d'objet détecté, 1: objet détecté

int interfaceVL6180x_litUneDistance(float *Distance);
int interfaceVL6810x_initialise(uint8_t fdp);

#endif

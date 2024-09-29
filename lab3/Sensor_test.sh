#!/bin/bash

SENSOR_ADDR_RECEIVE="$1"
SENSOR_REG_ID_ADDR_RECEIVE="$2"
SENSOR_ID_RECEIVE="$3"

# Validation de la presence du capteur
if  i2cdetect -y -r 2 $SENSOR_ADDR_RECEIVE $SENSOR_ADDR_RECEIVE | grep -q UU  ; then
	echo "Capteur a l'adresse $SENSOR_ADDR_RECEIVE est présent"
	
	#Validation du registre ID du capteur
	SENSOR_ID=$(i2cget -f -y 2 $SENSOR_ADDR_RECEIVE $SENSOR_REG_ID_ADDR_RECEIVE)
	if [[ "$SENSOR_ID" == "$SENSOR_ID_RECEIVE" ]] ; then
		echo "Capteur ID $SENSOR_ID correctement identifier"
	else
		echo "ERREUR : Capteur ID $SENSOR_ID non identifie"
	fi
else
	 echo "Capteur a l'adresse $SENSOR_ADDR_RECEIVE n'est pas detecte"
fi



#ifndef UARM_SWIFT_PRO_H
#define UARM_SWIFT_PRO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <math.h>

int init_serial(const char *port_name, int baud_rate);
void send_gcode(int serial_port, const char *command);
void get_current_coordonnates(int serial_port, int *x_rounded, int *y_rounded, int *z_rounded);
int arm_still_moving(int serial_port);
int limit_switch(int serial_port);
int grab_object(int serial_port);
void pump_on(int serial_port);
void pump_off(int serial_port);
void delay(int serial_port,int idelay);
int main_();
#endif


#ifndef UARM_SWIFT_PRO_H
#define UARM_SWIFT_PRO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>

int init_serial(const char *port_name, int baud_rate);
void send_gcode(int serial_port, const char *command);

#endif


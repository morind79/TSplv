#ifndef TELEINFO_H
#define TELEINFO_H

#include <Arduino.h>

char chksum(char *buff, uint8_t len);
void traitbuf_cpt(char *buff, uint8_t len);
void read_teleinfo();
unsigned long readIndexTI(int channel);

#endif
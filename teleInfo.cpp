#include <TeleInfo.h>
#include "display.h"

#define debtrame 0x02
#define debligne 0x0A
#define finligne 0x0D

volatile unsigned long prevHC = 0;
volatile unsigned long prevHP = 0;
volatile unsigned long currHC = 0;
volatile unsigned long currHP = 0;

volatile unsigned long index1 = 0;    // Index option Tempo - Heures Creuses Jours Bleus, Wh
volatile unsigned long index2 = 0;    // Index option Tempo - Heures Pleines Jours Bleus, Wh
volatile unsigned long index3 = 0;    // Index option Tempo - Heures Creuses Jours Blancs, Wh
volatile unsigned long index4 = 0;    // Index option Tempo - Heures Pleines Jours Blancs, Wh
volatile unsigned long index5 = 0;    // Index option Tempo - Heures Creuses Jours Rouges, Wh
volatile unsigned long index6 = 0;    // Index option Tempo - Heures Pleines Jours Rouges, Wh

byte inByte = 0;
uint8_t presence_teleinfo = 0;  // si signal teleinfo présent
char buffteleinfo[21] = "";
byte bufflen = 0;
byte num_abo = 0;

///////////////////////////////////////////////////////////////////
// Calculate Checksum
///////////////////////////////////////////////////////////////////
char chksum(char *buff, uint8_t len) {
  int i;
  char sum = 0;
    for (i=1; i<(len-2); i++) {
      sum = sum + buff[i];
    }
    sum = (sum & 0x3F) + 0x20;
    return(sum);
}

///////////////////////////////////////////////////////////////////
// Analyse de la ligne de Teleinfo
///////////////////////////////////////////////////////////////////
void traitbuf_cpt(char *buff, uint8_t len) {
  char optarif[4] = "";    // BASE, HC, EJP BBRx options

  if (num_abo == 0) { // détermine le type d'abonnement
    if (strncmp("OPTARIF ", &buff[1] , 8) == 0) {
      strncpy(optarif, &buff[9], 3);
      optarif[3]='\0';
      if (strcmp("BAS", optarif) == 0) {
        num_abo = 1;
      }
      else if (strcmp("HC.", optarif) == 0) {
        num_abo = 2;
      }
      else if (strcmp("EJP", optarif) == 0) {
        num_abo = 3;
      }
      else if (strcmp("BBR", optarif) == 0) {
        num_abo = 4;
      }
    }
  }
  else {
    if (num_abo == 1) {
      if (strncmp("BASE ", &buff[1] , 5) == 0) {
          index1 = atol(&buff[6]);
      }
    }
    else if (num_abo == 2) {
      if (strncmp("HCHP ", &buff[1] , 5) == 0) {
          index2 = atol(&buff[6]);
          currHP = index2;
          updateHP(currHP - prevHP);
      }
      else if (strncmp("HCHC ", &buff[1] , 5) == 0) {
          index1 = atol(&buff[6]);
          currHC = index1;
          updateHC(currHC - prevHC);
      }
    }
    else if (num_abo == 3) {
      if (strncmp("EJPHN ", &buff[1] , 6) == 0) {
          index1 = atol(&buff[7]);
      }
      else if (strncmp("EJPHPM ", &buff[1] , 7) == 0) {
          index2 = atol(&buff[8]);
      }
    }
    else if (num_abo == 4) {
      if (strncmp("BBRHCJB ", &buff[1] , 8) == 0) {
          index1 = atol(&buff[9]);
      }
      else if (strncmp("BBRHPJB ", &buff[1] , 8) == 0) {
          index2 = atol(&buff[9]);
      }
      else if (strncmp("BBRHCJW ", &buff[1] , 8) == 0) {
          index3 = atol(&buff[9]);
      }
      else if (strncmp("BBRHPJW ", &buff[1] , 8) == 0) {
          index4 = atol(&buff[9]);
      }
      else if (strncmp("BBRHCJR ", &buff[1] , 8) == 0) {
          index5 = atol(&buff[9]);
      }
      else if (strncmp("BBRHPJR ", &buff[1] , 8) == 0) {
          index6 = atol(&buff[9]);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////
// Lecture trame teleinfo (ligne par ligne)
/////////////////////////////////////////////////////////////////// 
void read_teleinfo()
{
  presence_teleinfo++;
  if (presence_teleinfo > 250) presence_teleinfo = 0;
  // si une donnée est dispo sur le port série
  if (Serial4.available() > 0) 
  {
    presence_teleinfo = 0;
    // recupère le caractère dispo
    inByte = Serial4.read();
    inByte = inByte & 0x7F;

    if (inByte == debtrame) bufflen = 0; // test le début de trame
    if (inByte == debligne) // test si c'est le caractère de début de ligne
    {
      bufflen = 0;
    }  
    buffteleinfo[bufflen] = inByte;
    bufflen++;
    if (bufflen > 21) {
      bufflen = 0;
    }
    if (inByte == finligne && bufflen > 5) // si Fin de ligne trouvée 
    {
      if (chksum(buffteleinfo, bufflen-1) == buffteleinfo[bufflen-2]) // Test du Checksum
      {
        traitbuf_cpt(buffteleinfo, bufflen-1); // ChekSum OK => Analyse de la Trame
      }
    }
  } 
}

unsigned long readIndexTI(int channel) {
  // Get HC
  if (channel == 0x01) {
    unsigned long ind = currHC - prevHC;
    return(ind);
  }
  // Get HP
  else if (channel == 0x02) {
    unsigned long ind = currHP - prevHP;
    return(ind);
  }
  // Reset all
  else if (channel == 0x32) {
    prevHC = currHC;
    prevHP = currHP;
  }
  return(0);
}
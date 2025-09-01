/*
 * Shutter program for Teesy 4.1
 *
 * Version : 2024-Sep-12
 *
 * Port A Input
 *  PA  | Room            | Teensy | PA  | Room          | Teensy
 * --------------------------------------------------------------
 *  A0  | motion door     | D28    | A1  | Motion cars   | D29
 *  A2  | Not used        | D30    | A3  | Water meter   | D31
 *  A4  | Garage Cecile   | D32    | A5  | Garage Denis  | D33
 *  A6  | IR Barrier      | D34    | A7  | Push button   | D35
 * 
 * Port B Output
 *  PB  | Room            | Teensy | PB  | Room          | Teensy
 * --------------------------------------------------------------
 *  B0  |                 | D2     | B1  |               | D3
 *  B2  |                 | D4     | B3  |               | D5
 *  B4  | Dry towel Cecil | D6     | B5  | Outside light | D7
 *  B6  | Dry towel Denis | D8     | B7  | Garage light  | D9
 * 
 * Port 220
 *  PC  | Room         | Teensy
 * ----------------------------
 *  C0  | Salon up     | D14 
 * 
 * 74LVC4245 direction control *******************************
 * PortA  DirA | D39
 * PortB  DirB | D38
 * 
 * GSM
 * RI  | D23
 * PWK | D17
 * 
 * EnergyMeters
 * Linky | D16
 * Cpy1  | D40
 * Cpy2  | D41
 * Cpy3  | D26
 * Cpy4  | D27
 * 
 * LCD
 * CS    | D10
 * D/C   | D15
 * BL    | D21
 */
#ifndef MAIN_H
#define MAIN_H

#define _TASK_INLINE  // To avoid include it from more than one cpp

#define FAKE false

char DENIS[] = "+33626281284";
int water = 0;                         // Water volume measured
int alarmActive = 0;                   // 0 -> Alarm not active, 1 -> Alarme active
unsigned long prevAlarm1;              // Variable used for knowing time since last alarm1
unsigned long prevAlarm2;              // Variable used for knowing time since last alarm2
unsigned long timeBetweenSMS = 50000;  // Waiting time before sending another SMS in ms
int powerFail = 0;                     // 0 -> No power, 1 -> Power ok
int peopleAtHomeS1 = 0;                // 0 when nobody there, 1 when motion detector detect something
int peopleAtHomeS2 = 0;                // 0 when nobody there, 1 when motion detector detect something
int lux = 0;                           // Light measured in lux
int limitLux = 100;                    // Light limit under which we can light up

// PortA
int portA[] = {28, 29, 30, 31, 32, 33, 34, 35};
int dirA = 39;
// PortB
int portB[] = {2, 3, 4, 5, 6, 7, 8, 9};
int dirB = 38;
// Port C
int portC[] = {14};

// GSM
int ri = 23;
int pwk = 17;

// TFT
int tftBL = 21;

// Energy meters
int emHome = 16;
int emProd = 26;
int emECS = 27;
int emPAC = 40;
int emAC = 41;

// GPS
extern byte gpsTimeZone;           // -12 to +12 (1 for Paris)
extern byte gpsTimeDST;            // 0 or 1 (1 to adjust DST automatically)
extern byte gpsTimeAutomatic;      // 0 or 1

// DST
extern byte summerDSTDate;
extern byte summerDSTMonth;
extern byte winterDSTDate;
extern byte winterDSTMonth;

void doReboot();
void synchronizeTime();
void sendSMS();
void alarm();
void sensors();
void gsm();
bool taskLightInsideOn();
void taskLightInsideOff();
bool taskLightOutsideOn();
void taskLightOutsideOff();
bool taskDryTowel1On();
void taskDryTowel1Off();
bool taskDryTowel2On();
void taskDryTowel2Off();

char *decodeSMS(const char *number, const char *text);
void doCommand(char *str);
char *concat(int count, ...);
int numCommand(const char *str);
char *GetCommand(const char *str, int index);
int GetTokenOpenBraces(char *input, int index);
int GetTokenCloseBraces(char *input, int index);
char *substring(char *string, int position, int length);
void initEthernet();
void recordEnergyMeter();

#endif
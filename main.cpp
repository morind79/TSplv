/*
 * Shutter program for Teesy 4.1
 *
 * Version : 2024-Sep-12
 *
 */
#include <Arduino.h>
#include <time.h>
#include <TimeLib.h>
#include <Wire.h>
#include <TaskScheduler.h>
#include "defines.h"  // Must be first
#include "teleInfo.h"
#include <QNEthernet.h>
#include <MySQL_Generic.h>
#include <pushButton.h>
#include <ILI9341_t3n.h>
#include "SPI.h"
#include "display.h"
#include "SIM7600.h"
#include "sensors.h"
#include "main.h"
#include "Watchdog_t4.h"

WDT_T4<WDT1> wdt;

SIM7600 sim7600 = SIM7600(21);
HardwareSerial *SIM7600Serial = &Serial1;

byte i2cCmd = 0x15;

// MySQL server
IPAddress server(192, 168, 1, 50);
uint16_t server_port = 3306;
char user[] = "morind79";             // MySQL user login username
char password[] = "DMO@moulins79";    // MySQL user login password
MySQL_Connection conn((Client *)&client);

// GPS
byte gpsTimeZone = 1;           // -12 to +12 (1 for Paris)
byte gpsTimeDST = 1;            // 0 or 1 (1 to adjust DST automatically)
byte gpsTimeAutomatic = 1;      // 0 or 1

volatile bool syncDone = false;
volatile bool recordDone = false;
volatile bool dryTowel1Done = false;
volatile bool dryTowel2Done = false;

bool receivedSMS = false;
char messageSMS[128];

// Energy meters
volatile long cntProd = 0;
volatile long cntECS = 0;
volatile long cntPAC = 0;
volatile long cntAC = 0;

unsigned char serial4buffer[2000];

boolean waterDone = false;

pushButton lineProd(emProd);
pushButton lineECS(emECS);
pushButton linePAC(emPAC);
pushButton lineAC(emAC);

char SIM7600InBuffer[64]; // For notifications from the FONA
char callerIDbuffer[32];  // We'll store the SMS sender number in here
char SMSbuffer[32];       // We'll store the SMS content in here
uint16_t SMSLength;
String SMSString = "";

int indexTempo = 0;

Scheduler runner;
Task tAlarm(100, TASK_FOREVER, &alarm, &runner, true);
Task tGSM(100, TASK_FOREVER, &gsm, &runner, true);
Task tSyncGPS(500, TASK_ONCE, &synchronizeTime, &runner, false);
Task tRecordEMeter(1000, TASK_FOREVER, &recordEnergyMeter, &runner, true);
Task tPulseLightInside(60 * TASK_SECOND, TASK_ONCE, NULL, &runner, false, &taskLightInsideOn, &taskLightInsideOff);    // Delay 60s for garage light
Task tPulseLightOutside(60 * TASK_SECOND, TASK_ONCE, NULL, &runner, false, &taskLightOutsideOn, &taskLightOutsideOff); // Delay 60s for outside light
Task tPulseDryTowel1(30 * TASK_MINUTE, TASK_ONCE, NULL, &runner, false, &taskDryTowel1On, &taskDryTowel1Off);
Task tPulseDryTowel2(30 * TASK_MINUTE, TASK_ONCE, NULL, &runner, false, &taskDryTowel2On, &taskDryTowel2Off);

bool taskLightInsideOn() {
  char msg[128];
  if (digitalRead(portB[7]) == LOW) {
    sprintf(msg, "%02d:%02d:%02d - Light inside ON", hour(), minute(), second());
    //addMessage(msg, ILI9341_AZURE);
  }
  digitalWrite(portB[7], HIGH);
  return true; // Task should be enabled
}

void taskLightInsideOff() {
  char msg[128];
  sprintf(msg, "%02d:%02d:%02d - Light inside OFF", hour(), minute(), second());
  //addMessage(msg, ILI9341_AZURE);
  digitalWrite(portB[7], LOW);
}

bool taskLightOutsideOn() {
  char msg[128];
  if (digitalRead(portB[5]) == LOW) {
    sprintf(msg, "%02d:%02d:%02d - Light outside ON", hour(), minute(), second());
    //addMessage(msg, ILI9341_AZURE);
  }
  digitalWrite(portB[5], HIGH);
  return true; // Task should be enabled
}

void taskLightOutsideOff() {
  char msg[128];
  digitalWrite(portB[5], LOW);
  sprintf(msg, "%02d:%02d:%02d - Light outside OFF", hour(), minute(), second());
  //addMessage(msg, ILI9341_AZURE);
}

// GSM
void gsm() {
  char* bufPtr = SIM7600InBuffer; // Handy buffer pointer
  char* status;
  char msg[128];

  if (sim7600.available()) { // Any data available from the SIM7600
    int slot = 0; // This will be the slot number of the SMS
    unsigned int charCount = 0;

#if FAKE
    digitalWrite(portB[1], !digitalRead(portB[1]));
#endif

    // Read the notification into fonaInBuffer
    do {
      *bufPtr = sim7600.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (sim7600.available()) && (++charCount < (sizeof(SIM7600InBuffer) - 1)));

    // Add a terminal NULL to the notification string
    *bufPtr = 0;

    // Scan the notification string for an SMS received notification.
    // If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(SIM7600InBuffer, "+CMTI: \"SM\",%d", &slot)) {
      sprintf(msg, "Slot : %d", slot);
      // Length   123456789ABCDFGHIJKLMNOPQRSTUVWXYZ1234
      addMessage(msg, ILI9341_GREEN);
      // Retrieve SMS sender address/phone number.
      if (!sim7600.getSMSSender(slot, callerIDbuffer, 31)) {
        // Length   123456789ABCDFGHIJKLMNOPQRSTUVWXYZ1234
        addMessage("Didn't find SMS message in slot !", ILI9341_RED);
      }
      sprintf(msg, "From : %s", callerIDbuffer);
      addMessage(msg, ILI9341_VIOLET);

      if (!sim7600.readSMS(slot, SMSbuffer, 250, &SMSLength)) { // pass in buffer and max len!
        addMessage("Read SMS failed !", ILI9341_RED);
      }
      else {
        SMSString = String(SMSbuffer);
        // Must not be more than 128 chars
        SMSString = SMSString.substring(0, 127);
        sprintf(msg, "%02d:%02d:%02d - SMS : %s", hour(), minute(), second(), SMSString.c_str());
        addMessage(msg, ILI9341_GREEN);
      }

      status = decodeSMS(callerIDbuffer, SMSString.c_str());
      if (strcmp(status, "") != 0) {
        if (!sim7600.sendSMS(DENIS, status)) {
          addMessage("Failed to send SMS !", ILI9341_RED);
        }
        else {
          // Length   123456789ABCDFGHIJKL
          addMessage("SMS Sent", ILI9341_GREEN);
        }
      }
      // Delete the original msg after it is processed otherwise, we will fill up all the slots and then we won't be able to receive SMS anymore
      while (1) {
        boolean deleteSMSDone = sim7600.deleteSMS(slot);
        if (deleteSMSDone == true) {
          Serial.println("OK!");
          break;
        }
        else {
          Serial.println("Couldn't delete, try again.");
        }
      }
    }
  }
  sendSMS();
}

// Task Sensors
void sensors() {
  // Motion detector + garage doors ******************************************
  if ((digitalRead(portA[0]) == LOW) || (digitalRead(portA[1]) == LOW) || (digitalRead(portA[4]) == HIGH) || (digitalRead(portA[5]) == HIGH)) {
    // Somebody at home
    peopleAtHomeS1 = 1;
    peopleAtHomeS2 = 1;
    // If light is not enough
    if (lux < limitLux) {
      if (digitalRead(portB[7]) == LOW) {
        tPulseLightInside.restartDelayed();
      }
    }
  }
  // IR barrier
  if (digitalRead(portA[6]) == 1) {
    // If light is not enough
    if (lux < limitLux) {
      if (digitalRead(portB[5]) == LOW) {
        tPulseLightOutside.restartDelayed();
      }
    }
  }
}

// Task Alarm
void alarm() {
  char msg[128];
  // Port A0 = motion detector service door **********************************
  if ((digitalRead(portA[0]) == 0) && (alarmActive == 1)) {
    if (millis() - prevAlarm1 >= timeBetweenSMS) {
      strcpy(messageSMS, "Detection mouvement porte de service");
      prevAlarm1 = millis();
    }
  }
  // Port A1 = motion detector cars ******************************************
  if ((digitalRead(portA[1]) == 0) && (alarmActive == 1)) {
    if (millis() - prevAlarm2 >= timeBetweenSMS) {
      strcpy(messageSMS, "Detection mouvement voiture");
      prevAlarm2 = millis();
    }
  }
  // Port A2 = detecteur zone 3 **********************************************

  // Port A3 = water meter ***************************************************
  if ((digitalRead(portA[3]) == LOW) && (waterDone == false)) {
    water++;
    waterDone = true;
  }
  else if ((digitalRead(portA[3]) == HIGH) && (waterDone == true)) {
    waterDone = false;
  }
  // Port A4 garage door *****************************************************
  if ((digitalRead(portA[4]) == 1) && (alarmActive == 1)) {
    if (millis() - prevAlarm2 >= timeBetweenSMS) {
      strcpy(messageSMS, "Portail Cecile ouvert");
      prevAlarm2 = millis();
    }
  }
  // Port A5 garage door *****************************************************
  if ((digitalRead(portA[5]) == 1) && (alarmActive == 1)) {
    if (millis() - prevAlarm2 >= timeBetweenSMS) {
      strcpy(messageSMS, "Portail Denis ouvert");
      prevAlarm2 = millis();
    }
  }
  // Port A6 = IR Barrier detector *******************************************
  if ((digitalRead(portA[6]) == 1) && (alarmActive == 1)) {
    if (millis() - prevAlarm2 >= timeBetweenSMS) {
      strcpy(messageSMS, "IR Barrier");
       prevAlarm2 = millis();
    }
  }
  // Port C0 = Grid power failure ********************************************
  if ((digitalRead(portC[0]) == 1) && (powerFail == 0)) {
    if (millis() - prevAlarm2 >= timeBetweenSMS) {
      strcpy(messageSMS, "Grid power back");
      powerFail = 1;
      prevAlarm2 = millis();
      sprintf(msg, "%02d:%02d:%02d - Grid power back", hour(), minute(), second());
      addMessage(msg, ILI9341_CYAN);
    }
  }
  if ((digitalRead(portC[0]) == 0) && (powerFail == 1)) {
    if (millis() - prevAlarm2 >= timeBetweenSMS) {
      strcpy(messageSMS, "Grid power failure");
      powerFail = 0;
      prevAlarm2 = millis();
      sprintf(msg, "%02d:%02d:%02d - Grid power failure", hour(), minute(), second());
      addMessage(msg, ILI9341_CYAN);
    }
  }
  updateTime();
  updateWater(water);
  indexTempo++;
  // Only every 60 * delay
  if (indexTempo > 10) {
    lux = readGY30();
    updateLux(lux, limitLux);
    indexTempo = 0;
    if (lux < 10) {
      digitalWrite(tftBL, LOW);
    }
    else {
      digitalWrite(tftBL, HIGH);
    }
  }
}

void initEthernet() {
  Ethernet.begin(myIP, myNetmask, myGW);
  Ethernet.setDNSServerIP(mydnsServer);
  if (!Ethernet.waitForLocalIP(5000)) {
    // Length   123456789ABCDFGHIJKL
    addMessage("Ethernet initialization failed !", ILI9341_RED);

    if (!Ethernet.linkStatus()) {
      // Length   123456789ABCDFGHIJKL
      addMessage("Check RJ45 cable !", ILI9341_RED);
    }

    // Stay here forever
    while (true) {
      delay(1);
    }
  }

  if (!Ethernet.waitForLink(5000)) {
    // Length   123456789ABCDFGHIJKL
    addMessage("No ethernet link !", ILI9341_RED);
  }
  else {
    char sIP[128];
    IPAddress add = Ethernet.localIP();
    snprintf(sIP, sizeof(sIP),"IP : %d.%d.%d.%d", add[0], add[1], add[2], add[3]);
    // Length   123456789ABCDFGHIJKL
    addMessage(sIP, ILI9341_GREEN);
  }
}

void counter1() {
  cntProd = cntProd + 1;
  updateProd(cntProd);
}

void counter2() {
  cntECS = cntECS + 1;
  updateECS(cntECS);
}

void counter3() {
  cntPAC = cntPAC + 1;
  updatePAC(cntPAC);
}

void counter4() {
  cntAC = cntAC + 1;
  updateAC(cntAC);
}

void resetIndex() {
  cntProd = 0;
  cntECS = 0;
  cntPAC = 0;
  cntAC = 0;
}

// INSERT INTO EnergyMeters (Date,Maison) VALUES (2024-01-02,'12345');
void recordEnergyMeter() {
  unsigned long indexHC = 0;
  unsigned long indexHP = 0;
  char qry[400] = "";
  char msg[128] = "";
  if ((recordDone == true) && (hour() == 0) && (minute() == 15)) {
    recordDone = false;
  }
  if ((recordDone == false) && (hour() == 0) && (minute() == 10)) {
    recordDone = true;
    sprintf(msg, "%02d:%02d:%02d", hour(), minute(), second());
    // Length   123456789ABCDFGHIJKL
    addMessage(msg, ILI9341_CYAN);
    row_values *row = NULL;
    // Length   123456789ABCDFGHIJKL
    addMessage("Connect to DB", ILI9341_GREEN);
    if (conn.connectNonBlocking(server, server_port, user, password) != RESULT_FAIL)
    {
      delay(500);
      if (conn.connected())
      {
        // Initiate the query class instance
        MySQL_Query query_mem = MySQL_Query(&conn);
#if FAKE
    sprintf(qry, "SELECT * FROM Domotic.Fake WHERE Date = CURDATE() - INTERVAL 1 DAY;");
#else
    sprintf(qry, "SELECT * FROM Domotic.EnergyMeters WHERE Date = CURDATE() - INTERVAL 1 DAY;");
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (Check date)", ILI9341_RED);
          return;
        }
        // Fetch the columns (required) but we don't use them.
        query_mem.get_columns();
        // Length   123456789ABCDFGHIJKL
        addMessage("Check if we need to add row", ILI9341_GREEN);
        row = query_mem.get_next_row();
        if (row == NULL) {
          // Length   123456789ABCDFGHIJKL
          addMessage("Add row in DB", ILI9341_GREEN);
          // If no record, we prepare it with only the date set
#if FAKE
    sprintf(qry, "INSERT INTO Domotic.Fake (Date) VALUES (CURDATE() - INTERVAL 1 DAY);");
#else
    sprintf(qry, "INSERT INTO Domotic.EnergyMeters (Date) VALUES (CURDATE() - INTERVAL 1 DAY);");
#endif
          if (!query_mem.execute(qry))  {
            // Length   123456789ABCDFGHIJKL
            addMessage("Query error (Insert date)", ILI9341_RED);
            return;
          }
        }
        else {
          // Length   123456789ABCDFGHIJKL
          addMessage("Row already present", ILI9341_GREEN);
        }

        // channel 1 = Production **********************************************
#if FAKE
    sprintf(qry, "UPDATE Domotic.Fake SET Production='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntProd);
#else
    sprintf(qry, "UPDATE Domotic.EnergyMeters SET Production='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntProd);
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (Set Production)", ILI9341_RED);
          return;
        }
        sprintf(msg, "Prod = %lu Wh", cntProd);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
        cntProd = 0;
        updateProd(cntProd);

        // channel 2 = ECS *****************************************************
#if FAKE
    sprintf(qry, "UPDATE Domotic.Fake SET ECS='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntECS);
#else
    sprintf(qry, "UPDATE Domotic.EnergyMeters SET ECS='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntECS);
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (Set ECS)", ILI9341_RED);
          return;
        }
        sprintf(msg, "ECS = %lu Wh", cntECS);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
        cntECS = 0;
        updateECS(cntECS);

        // channel 3 = PAC ******************************************************
#if FAKE
    sprintf(qry, "UPDATE Domotic.Fake SET PAC='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntPAC);
#else
    sprintf(qry, "UPDATE Domotic.EnergyMeters SET PAC='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntPAC);
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (Set PAC)", ILI9341_RED);
          return;
        }
        sprintf(msg, "PAC = %lu Wh", cntPAC);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
        cntPAC = 0;
        updatePAC(cntPAC);

        // channel 4 = Production ************************************************
#if FAKE
    sprintf(qry, "UPDATE Domotic.Fake SET AutoConsommation='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntAC);
#else
    sprintf(qry, "UPDATE Domotic.EnergyMeters SET AutoConsommation='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", cntAC);
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (set AutoCons)", ILI9341_RED);
          return;
        }
        sprintf(msg, "AutoCons = %lu Wh", cntAC);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
        cntAC = 0;
        updateAC(cntAC);

        // Water meter **********************************************************
#if FAKE
    sprintf(qry, "UPDATE Domotic.Fake SET Eau='%d' WHERE Date = CURDATE() - INTERVAL 1 DAY;", water);
#else
    sprintf(qry, "UPDATE Domotic.EnergyMeters SET Eau='%d' WHERE Date = CURDATE() - INTERVAL 1 DAY;", water);
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (Set water)", ILI9341_RED);
          return;
        }
        sprintf(msg, "Eau = %d l", water);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
        water = 0;

        // Energy meter TI ***************************************************
        // HC = 0x01
        indexHC = readIndexTI(0x01);
        sprintf(msg, "HC = %lu Wh", indexHC);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
        // HP = 0x02
        indexHP = readIndexTI(0x02);
        sprintf(msg, "HP = %lu Wh", indexHP);
        // Length   123456789ABCDFGHIJKL
        addMessage(msg, ILI9341_CYAN);
#if FAKE
    sprintf(qry, "UPDATE Domotic.Fake SET MaisonHC='%lu', Maison='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", indexHC, indexHP);
#else
    sprintf(qry, "UPDATE Domotic.EnergyMeters SET MaisonHC='%lu', Maison='%lu' WHERE Date = CURDATE() - INTERVAL 1 DAY;", indexHC, indexHP);
#endif
        if (!query_mem.execute(qry))  {
          // Length   123456789ABCDFGHIJKL
          addMessage("Query error (Set HP/HC)", ILI9341_RED);
          return;
        }
        // Reset all counters
        readIndexTI(0x32);
      }
      else {
        // Length   123456789ABCDFGHIJKL
        addMessage("Not connected to DB", ILI9341_RED);
      }
    }
    // Close connection
    conn.close();
    // Length   123456789ABCDFGHIJKL
    addMessage("Disconnect from DB", ILI9341_GREEN);
  }
}

void synchronizeTime() {
  int i = 0;
  float latitude, longitude, altitude;
  time_t gpsDT;
  char msg[128];
  boolean gps_success = sim7600.getGPS(&latitude, &longitude, &gpsDT, &altitude);
  while (!gps_success) {
    if (i > 150) {
      doReboot();
    }
    // Wait till we have a fix
    delay(2000);
    gps_success = sim7600.getGPS(&latitude, &longitude, &gpsDT, &altitude);
    i++;
  }
  addMessage("Sync date and time with GPS", ILI9341_GREEN);
  // Set Teensy time
  setTime(hour(gpsDT), minute(gpsDT), second(gpsDT), day(gpsDT), month(gpsDT), year(gpsDT));
  sprintf(msg, "%02d:%02d:%02d", hour(), minute(), second());
  addMessage(msg, ILI9341_GREEN);
  // Adjust DST last sunday of march
  int beginDSTDate = (31 - (5 * year()/4 + 4) % 7);
  int beginDSTMonth = 3;
  // Last sunday of october
  int endDSTDate = (31 - (5 * year()/4 + 1) % 7);
  int endDSTMonth = 10;
  // Adjustig Time with DST Europe/France/Paris: UTC+1h in winter, UTC+2h in summer
  if (((month() > beginDSTMonth) && (month() < endDSTMonth))
     || ((month() == beginDSTMonth) && (day() > beginDSTDate)) 
     || ((month() == beginDSTMonth) && (day() == beginDSTDate) && (hour() >= 1))
     || ((month() == endDSTMonth) && (day() < endDSTDate))
     || ((month() == endDSTMonth) && (day() == endDSTDate) && (hour() < 1)))
    adjustTime((gpsTimeZone + 1) * SECS_PER_HOUR);      // Summer time + 1 hour
  else adjustTime(gpsTimeZone * SECS_PER_HOUR);         // winter time
  updateDate();
  syncDone = false;
  tSyncGPS.disable();
}

bool taskDryTowel1On() {
  char msg[128];
  if (peopleAtHomeS1 == 1) {
    if (digitalRead(portB[4]) == LOW) {
      sprintf(msg, "%02d:%02d:%02d - Dry towel 1 ON", hour(), minute(), second());
      addMessage(msg, ILI9341_YELLOW);
    }
    digitalWrite(portB[4], HIGH);
    }
  return true; // Task should be enabled
}

void taskDryTowel1Off() {
  char msg[128];
  peopleAtHomeS1 = 0;
  if (digitalRead(portB[4]) == HIGH) {
    sprintf(msg, "%02d:%02d:%02d - Dry towel 1 OFF", hour(), minute(), second());
    addMessage(msg, ILI9341_YELLOW);
  }
  digitalWrite(portB[4], LOW);
}

bool taskDryTowel2On() {
  char msg[128];
  if (peopleAtHomeS1 == 1) {
    if (digitalRead(portB[6]) == LOW) {
      sprintf(msg, "%02d:%02d:%02d - Dry towel 2 ON", hour(), minute(), second());
      addMessage(msg, ILI9341_YELLOW);
    }
    digitalWrite(portB[6], HIGH);
  }
  return true; // Task should be enabled
}

void taskDryTowel2Off() {
  char msg[128];
  peopleAtHomeS1 = 0;
  if (digitalRead(portB[6]) == HIGH) {
    sprintf(msg, "%02d:%02d:%02d - Dry towel 2 OFF", hour(), minute(), second());
    addMessage(msg, ILI9341_YELLOW);
  }
  digitalWrite(portB[6], LOW);
}

void sendSMS() {
  if (strcmp(messageSMS, "") != 0) {
    if (!sim7600.sendSMS(DENIS, messageSMS)) {
      Serial.println("Failed");
      addMessage("Failed to send SMS", ILI9341_RED);
    }
    else {
      Serial.println(F("Sent!"));
      addMessage("SMS Sent", ILI9341_GREEN);
    }
    messageSMS[0] = '\0';
  }
}

char *decodeSMS(const char *number, const char *text) {
  int i;
  char *cmd;
  char *status;
  // Compare number to be sure this is an allowed message
  if (strcmp(number, DENIS) == 0) {
    // Message is (A=x)(B=y)...
    int numCmd = numCommand(text);
    if (numCmd > 0) {
      // Get all commands
      for (i=1; i<numCmd+1; i++) {
        cmd = GetCommand(text, i);
        doCommand(cmd);
      }
    }
    else {
      status = concat(0);
      // Get status of ... TBD

      // Send back status
      return(status);
    }
  }
  return(concat(0));
}

void doCommand(char *str) {
  char cmd = '0';
  char val = '\0';
  if (strlen(str) >= 3) {
    cmd = str[0];
    val = str[2];
    // Command is A=1 for example
    switch (cmd) {
      case 'A' : // Salon
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('a');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('A');
          Wire.endTransmission();
        }
        break;
      case 'B' : // SDB
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('b');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('B');
          Wire.endTransmission();
        }
        break;
      case 'C' : // Erine
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('c');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('C');
          Wire.endTransmission();
        }
        break;
      case 'D' : // Eva
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('d');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('D');
          Wire.endTransmission();
        }
        break;
      case 'E' : // Laura
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('e');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('E');
          Wire.endTransmission();
        }
       break;
      case 'F' : // SAM
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('f');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('F');
          Wire.endTransmission();
        }
        break;
      case 'G' : // Cuisine
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('g');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('G');
          Wire.endTransmission();
        }
        break;
      case 'H' : // Cellier
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('h');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('H');
          Wire.endTransmission();
        }
        break;
      case 'I' : // General
        if (val == '0') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('i');
          Wire.endTransmission();
        }
        else if (val == '1') {
          Wire.beginTransmission(i2cCmd);
          Wire.write('I');
          Wire.endTransmission();
        }
        break;
      case 'J' : // Alarm
        if (val == '0') {alarmActive = 0;}
        if (val == '1') {alarmActive = 1;}
        break;
    }
  }
}

char *concat(int count, ...) {
  va_list ap;
  int i;

  // Find required length to store merged string
  int len = 1; // room for NULL
  va_start(ap, count);
  for(i=0 ; i<count ; i++)
    len += strlen(va_arg(ap, char*));
  va_end(ap);

  // Allocate memory to concat strings
  char *merged = (char *)calloc(sizeof(char), len);
  int null_pos = 0;

  // Actually concatenate strings
  va_start(ap, count);
  for(i=0 ; i<count ; i++) {
    char *s = va_arg(ap, char*);
    strcpy(merged+null_pos, s);
    null_pos += strlen(s);
  }
  va_end(ap);

  return merged;
}

int numCommand(const char *str) {
  int i;
  int b = 0;
  for(i = 0; str[i] != '\0'; i++)   {
    if (str[i] == '(') {
      b++;
    }
  }
  return(b);
}

char *GetCommand(const char *str, int index) {
  int start = GetTokenOpenBraces(strdup(str), index);
  int end = GetTokenCloseBraces(strdup(str), index);
  if (start < end) {
    // Get what we need that in between braces
    char *res = substring(strdup(str), start+1, end-start-1);
    return(res);
  }
  else {
    return(concat(0));
  }
}

int GetTokenOpenBraces(char *input, int index) {
  int i;
  int b = 0;
  for(i = 0; input[i] != '\0'; i++) {
    if (input[i] == '(') {
      b++;
      if (b == index) return(i);
    }
  }
  return(0);
}

int GetTokenCloseBraces(char *input, int index) {
  int i;
  int b = 0;
  for(i = 0; input[i] != '\0'; i++)
  {
    if (input[i] == ')') {
      b++;
      if (b == index) return(i);
    }
  }
  return(0);
}

char *substring(char *string, int position, int length) {
   char *pointer;
   int c;
   pointer = (char *)malloc(length+1);
   if (pointer == NULL) {
      exit(EXIT_FAILURE);
   }
   for (c = 0; c < position; c++)
      string++;
   for (c = 0; c < length; c++) {
      *(pointer+c) = *string;
      string++;
   }
   *(pointer+c) = '\0';

   return pointer;
}

void doReboot() {
  for (int i = 0; i <= 7; i++) {
    digitalWrite(portB[i], LOW);
  }
  // Reboot
  SCB_AIRCR = 0x05FA0004;
}

// Setup *******************************************************************************
void setup() {
  Serial.begin(9600);
  Serial.println("Start");
  // Energy meter, even parity, 7 bit data
  Serial4.begin(1200, SERIAL_7E1);

  Serial4.addMemoryForRead(serial4buffer, sizeof(serial4buffer));

  // LCD
  initDisplay();
  addMessage("Start V250515", ILI9341_GREEN);
  // Watchdog
  WDT_timings_t config;
  config.trigger = 30; /* in seconds, 0->128 */
  config.timeout = 60; /* in seconds, 0->128 */
  config.callback = doReboot;
  wdt.begin(config);
  // Port A, B and C
  for (int i = 0; i <= 7; i++) {
    pinMode(portA[i], INPUT_PULLDOWN);
    pinMode(portB[i], OUTPUT);
    digitalWrite(portB[i], LOW);
  }
  pinMode(portC[0], INPUT_PULLDOWN);
  // 74LVC4245 direction
  digitalWrite(dirA, HIGH);  // Input (A data to B bus on 74LVC4245)
  digitalWrite(dirB, LOW);   // Output (B data to A bus on 74LVC4245)
  // Energy meters
  pinMode(emProd, INPUT);
  pinMode(emECS, INPUT);
  pinMode(emPAC, INPUT);
  pinMode(emAC, INPUT);
  messageSMS[0] = '\0';
  // Init of timers
  prevAlarm1 = millis();
  prevAlarm2 = millis();
  pinMode(tftBL, OUTPUT);
  digitalWrite(tftBL, HIGH);
  // I2C
  //Wire.begin();
  Wire2.begin();
  powerOnSensors();
  // Ethernet
  initEthernet();
  // Update counters
  updateProd(cntProd);
  updatePAC(cntPAC);
  updateECS(cntECS);
  updateHP(0);
  updateHC(0);
  updateAC(cntAC);
  // Set time to be 00:00:00 1-Jan-2024
  setTime(0, 0, 0, 1, 1, 24);

  SIM7600Serial->begin(115200);
  if (!sim7600.begin(*SIM7600Serial)) {
    // Length   123456789ABCDFGHIJKL
    addMessage("Can't find SIM7600 module", ILI9341_RED);
    while (1);
  }
  
  SIM7600Serial->print("AT+CNMI=2,1\r\n");  // Set up to send a +CMTI notification when an SMS is received

  sim7600.enableGPS(true);

  lux = 1000;
  // Task
  runner.startNow();
  // Get GPS time
  tSyncGPS.enable();
}

// Main loop ***************************************************************************
void loop() {
  // To let know watchdog we are still alive
  static uint32_t callback_test = millis();
  if ( millis() - callback_test > 5500 ) {
    callback_test = millis();
    wdt.feed();
  }
  // ************************************************************************************************
  // Sync date and time *****************************************************************************
  if ((syncDone == false) && (hour() == 2) && (minute() == 10)) { // 2h00 -> 0h00
    syncDone = true;
    addMessage("Enable GPS Date/time synchronization", ILI9341_GREEN);
    tSyncGPS.restartDelayed(60000);
  }
  // *************************************************************************************************
  // Dry towels **************************************************************************************
  if ((dryTowel1Done == false) && (hour() == 1) && (minute() == 0)) { // 1h00 -> 23h00
    dryTowel1Done = true;
    tPulseDryTowel1.restartDelayed();
  }
  if ((dryTowel1Done == true) && (hour() == 1) && (minute() == 30)) { // 1h30 -> 23h30
    dryTowel1Done = false;
  }
  if ((dryTowel2Done == false) && (hour() == 3) && (minute() == 0)) { // 3h00 -> 1h00
    dryTowel2Done = true;
    tPulseDryTowel2.restartDelayed();
  }
  if ((dryTowel2Done == true) && (hour() == 3) && (minute() == 30)) { // 3h30 -> 1h30
    dryTowel2Done = false;
  }

  // *************************************************************************************************
  // Read sensors ************************************************************************************
  sensors();
  
  // *************************************************************************************************
  // Energy meters ***********************************************************************************
  if(lineProd.wasPressed()) {
    counter1();
  }
  if(lineECS.wasPressed()) {
    counter2();
  }
  if(linePAC.wasPressed()) {
    counter3();
  }
  if(lineAC.wasPressed()) {
    counter4();
  }
  read_teleinfo();
  
  // Task
  runner.execute();
}
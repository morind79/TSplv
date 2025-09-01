/*
 * TSplc_v1 for Teesy 4.1
 * Handle ILI9341 display
 * Version : 2024-Sep-12
 *
 */
#include <Arduino.h>
#include "SPI.h"
#include "display.h"
#include <TimeLib.h>
#include <ILI9341_t3n.h>

#define NUMBER_OF_STRINGS1    29
#define STRING_LENGTH1        40

#define NUMBER_OF_STRINGS2    14
#define STRING_LENGTH2        20

typedef struct {
  char message[STRING_LENGTH1 + 1];
  uint16_t color;
}Console;

// Use hardware SPI for LCD
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC);

String monthArray[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char console1[NUMBER_OF_STRINGS1][STRING_LENGTH1 + 1];  // Array to handle console content
char console2[NUMBER_OF_STRINGS2][STRING_LENGTH2 + 1];  // Array to handle console content

Console term[NUMBER_OF_STRINGS1];

void initDisplay() {
  int y;
  tft.begin();
  tft.useFrameBuffer(1);
  tft.fillScreen(C_BACKGROUND);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.setTextColor(C_CONSOLE);
  tft.setTextSize(2);
  tft.println("TSplc_v1  2024-Oct");
  delay(1000);
  tft.fillScreen(C_BACKGROUND);
  // Draw table
  tft.drawLine(0, 15, 240, 15, C_TABLE); // Horizontal
  tft.drawLine(0, 32, 240, 32, C_TABLE); // Horizontal
  tft.drawLine(0, 49, 240, 49, C_TABLE); // Horizontal
  tft.drawLine(0, 66, 240, 66, C_TABLE); // Horizontal
  tft.drawLine(0, 83, 240, 83, C_TABLE); // Horizontal
  tft.drawLine(120, 15, 120, 83, C_TABLE); // Vertical
  // Water and Solar panels
  tft.setCursor(0, 17);
  tft.setTextColor(C_WATER);
  tft.print("EAU:");
  tft.setCursor(125, 17);
  tft.setTextColor(C_EMETER);
  tft.print("S:");
  // PAC and ECS
  tft.setCursor(0, 34);
  tft.setTextColor(C_EMETER);
  tft.print("PAC:");
  tft.setCursor(125, 34);
  tft.setTextColor(C_EMETER);
  tft.print("ECS:");
  // Home HP and HC
  tft.setCursor(0, 51);
  tft.setTextColor(C_EMETER);
  tft.print("HP:");
  tft.setCursor(125, 51);
  tft.setTextColor(C_EMETER);
  tft.print("HC:");
  // AutoConsommation and Lux
  tft.setCursor(0, 68);
  tft.setTextColor(C_EMETER);
  tft.print("AC:");
  tft.setCursor(125, 68);
  tft.setTextColor(C_LUX_LIGHT);
  tft.print("L:");
  tft.updateScreen();
  // Console
  for(y = 0; y < NUMBER_OF_STRINGS1; y++) {
    strcpy(console1[y], "");
  }
  for(y = 0; y < NUMBER_OF_STRINGS2; y++) {
    strcpy(console2[y], "");
  }
}

void updateTime() {
  tft.setCursor(0, 0);
  tft.setTextColor(C_TIME);
  tft.setTextSize(2);
  tft.fillRect(0, 0, 120, 14, C_BACKGROUNDTIMEDATE);
  // Time
  if (hour() < 10) tft.print("0");
  tft.print(hour());
  tft.print(":");
  if (minute() < 10) tft.print("0");
  tft.print(minute());
  tft.print(":");
  if (second() < 10) tft.print("0");
  tft.print(second());
  tft.updateScreen();
}

void updateDate() {
  tft.setCursor(165, 0);
  tft.setTextColor(C_DATE);
  tft.setTextSize(2);
  tft.fillRect(120, 0, 120, 14, C_BACKGROUNDTIMEDATE);
  // Date
  if (day() < 10) tft.print("0");
  tft.print(day());
  tft.print("-");
  tft.print(monthArray[month() - 1]);
  tft.updateScreen();
}

void updateWater(int waterVolume) {
  tft.setCursor(48, 17);
  tft.setTextColor(C_WATER);
  tft.setTextSize(2);
  tft.fillRect(48, 16, 72, 16, C_BACKGROUND);
  tft.print(waterVolume);
  tft.updateScreen();
}

void updateProd(int prod) {
  tft.setCursor(170, 17);
  tft.setTextColor(C_EMETER);
  tft.setTextSize(2);
  tft.fillRect(170, 16, 72, 16, C_BACKGROUND);
  tft.print(prod);
  tft.updateScreen();
}

void updatePAC(int pac) {
  tft.setCursor(48, 34);
  tft.setTextColor(C_EMETER);
  tft.setTextSize(2);
  tft.fillRect(48, 33, 72, 16, C_BACKGROUND);
  tft.print(pac);
  tft.updateScreen();
}

void updateECS(int ecs) {
  tft.setCursor(170, 34);
  tft.setTextColor(C_EMETER);
  tft.setTextSize(2);
  tft.fillRect(170, 33, 69, 16, C_BACKGROUND);
  tft.print(ecs);
  tft.updateScreen();
}

void updateHP(int hp) {
  if (abs(hp) > 1000000) {
    hp = 0;
  }
  tft.setCursor(48, 51);
  tft.setTextColor(C_EMETER);
  tft.setTextSize(2);
  tft.fillRect(48, 50, 72, 16, C_BACKGROUND);
  tft.print(hp);
  tft.updateScreen();
}

void updateHC(int hc) {
  if (abs(hc) > 1000000) {
    hc = 0;
  }
  tft.setCursor(170, 51);
  tft.setTextColor(C_EMETER);
  tft.setTextSize(2);
  tft.fillRect(170, 50, 69, 16, C_BACKGROUND);
  tft.print(hc);
  tft.updateScreen();
}

void updateAC(int ac) {
  tft.setCursor(48, 68);
  tft.setTextColor(C_EMETER);
  tft.setTextSize(2);
  tft.fillRect(48, 67, 72, 16, C_BACKGROUND);
  tft.print(ac);
  tft.updateScreen();
}

void updateLux(int lux, int limitLux) {
  tft.setCursor(146, 68);
  if (lux < limitLux) {
    tft.setTextColor(C_LUX_DARK);
  }
  else {
    tft.setTextColor(C_LUX_LIGHT);
  }
  tft.setTextSize(2);
  tft.fillRect(146, 67, 72, 16, C_BACKGROUND);
  tft.print(lux);
  tft.updateScreen();
}

// Display content of console from line 6 to 19
void refreshConsole() {
  int y;
  tft.fillRect(0, 84, 240, 236, C_BACKGROUND);
  tft.setCursor(0, 85);
  tft.setTextSize(1);
  for(y = 0; y < NUMBER_OF_STRINGS1; y++) {
    tft.setTextColor(term[y].color);
    tft.println(term[y].message);
  }
  // Update display
  tft.updateScreen();
}

void addMessage(char const *message, uint16_t msgcolor) {
  char msg[40];
  // Be sure message is not bigger than 39 charatcters
  if (strlen(message) > 39) {
    strncpy(msg, message, 38);
    msg[39] = '\0';
  }
  else {
    strcpy(msg, message);
  }
  // Shift all lines
  strcpy(term[0].message, term[1].message);
  strcpy(term[1].message, term[2].message);
  strcpy(term[2].message, term[3].message);
  strcpy(term[3].message, term[4].message);
  strcpy(term[4].message, term[5].message);
  strcpy(term[5].message, term[6].message);
  strcpy(term[6].message, term[7].message);
  strcpy(term[7].message, term[8].message);
  strcpy(term[8].message, term[9].message);
  strcpy(term[9].message, term[10].message);
  strcpy(term[10].message, term[11].message);
  strcpy(term[11].message, term[12].message);
  strcpy(term[12].message, term[13].message);
  strcpy(term[13].message, term[14].message);
  strcpy(term[14].message, term[15].message);
  strcpy(term[15].message, term[16].message);
  strcpy(term[16].message, term[17].message);
  strcpy(term[17].message, term[18].message);
  strcpy(term[18].message, term[19].message);
  strcpy(term[19].message, term[20].message);
  strcpy(term[20].message, term[21].message);
  strcpy(term[21].message, term[22].message);
  strcpy(term[22].message, term[23].message);
  strcpy(term[23].message, term[24].message);
  strcpy(term[24].message, term[25].message);
  strcpy(term[25].message, term[26].message);
  strcpy(term[26].message, term[27].message);
  strcpy(term[27].message, term[28].message);
  strcpy(term[28].message, msg);
  term[0].color = term[1].color;
  term[1].color = term[2].color;
  term[2].color = term[3].color;
  term[3].color = term[4].color;
  term[4].color = term[5].color;
  term[5].color = term[6].color;
  term[6].color = term[7].color;
  term[7].color = term[8].color;
  term[8].color = term[9].color;
  term[9].color = term[10].color;
  term[10].color = term[11].color;
  term[11].color = term[12].color;
  term[12].color = term[13].color;
  term[13].color = term[14].color;
  term[14].color = term[15].color;
  term[15].color = term[16].color;
  term[16].color = term[17].color;
  term[17].color = term[18].color;
  term[18].color = term[19].color;
  term[19].color = term[20].color;
  term[20].color = term[21].color;
  term[21].color = term[22].color;
  term[22].color = term[23].color;
  term[23].color = term[24].color;
  term[24].color = term[25].color;
  term[25].color = term[26].color;
  term[26].color = term[27].color;
  term[27].color = term[28].color;
  term[28].color = msgcolor;
  refreshConsole();
}

// Display content of console from line 6 to 19
void refreshConsole2() {
  int y;
  tft.fillRect(0, 84, 240, 236, C_BACKGROUND);
  tft.setCursor(0, 85);
  tft.setTextColor(C_CONSOLE);
  tft.setTextSize(2);
  for(y = 0; y < NUMBER_OF_STRINGS2; y++) {
    tft.println(console2[y]);
  }
  // Update display
  tft.updateScreen();
}

void addMessage2(char const *message) {
  // Shift all lines
  strcpy(console2[0], console2[1]);
  strcpy(console2[1], console2[2]);
  strcpy(console2[2], console2[3]);
  strcpy(console2[3], console2[4]);
  strcpy(console2[4], console2[5]);
  strcpy(console2[5], console2[6]);
  strcpy(console2[6], console2[7]);
  strcpy(console2[7], console2[8]);
  strcpy(console2[8], console2[9]);
  strcpy(console2[9], console2[10]);
  strcpy(console2[10], console2[11]);
  strcpy(console2[11], console2[12]);
  strcpy(console2[12], console2[13]);
  strcpy(console2[13], message);
  refreshConsole2();
}

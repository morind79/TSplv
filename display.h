/*
 * Shutter program for Teesy 4.1
 * Handle ILI9341 display
 * Version : 2024-Sep-12
 * 
 * LCD
 * CS    | D10
 * D/C   | D15
 * BL    | D21
 */

#ifndef DISPLAY_H
#define DISPLAY_H

// LCD
#define TFT_DC 15
#define TFT_CS 10
#define TFT_BL 21

// Colors
#define C_BACKGROUND ILI9341_BLACK
#define C_BACKGROUNDTIMEDATE ILI9341_BLUE
#define C_TABLE ILI9341_WHITE
#define C_WATER ILI9341_LIGHTSKYBLUE
#define C_TIME ILI9341_YELLOW
#define C_DATE ILI9341_YELLOW
#define C_EMETER ILI9341_RED
#define C_LUX_LIGHT ILI9341_YELLOW
#define C_LUX_DARK ILI9341_LIGHTSKYBLUE
#define C_CONSOLE ILI9341_GREEN

void initDisplay();
void updateTime();
void updateDate();
void updateWater(int waterVolume);
void updateLux(int lux, int limitLux);
void updateProd(int prod);
void updatePAC(int pac);
void updateECS(int ecs);
void updateHP(int hp);
void updateHC(int hc);
void updateAC(int ac);
void refreshConsole();
void addMessage(char const *message, uint16_t msgcolor);
void refreshConsole2();
void addMessage2(char const *message);

#endif
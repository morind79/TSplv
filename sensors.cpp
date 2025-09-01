#include <Arduino.h>
#include <BH1750FVI.h>

#define ADD_GY30 0x23

BH1750FVI gy30(ADD_GY30, &Wire2);

// ****************************************************************************
// ********************************* GY-30 ************************************
// ****************************************************************************
void powerOnSensors() {
  gy30.powerOn();
  gy30.setContHighRes();
}

float readGY30() {
  float lightLevel = gy30.getLux();
  return(lightLevel);
}


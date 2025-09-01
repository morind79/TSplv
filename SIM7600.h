/*
 * @file SIM7600.h
 */
#ifndef SIM7600_H
#define SIM7600_H

#include "Arduino.h"
#include <time.h>
#include <TimeLib.h>

// Set the preferred SMS storage.
//   Use "SM" for storage on the SIM.
//   Use "ME" for internal storage on the SIM7600 chip
#define PREF_SMS_STORAGE "\"SM\""

#define DEFAULT_TIMEOUT_MS 500

#define CALL_READY 0
#define CALL_FAILED 1
#define CALL_UNKNOWN 2
#define CALL_RINGING 3
#define CALL_INPROGRESS 4

#define DebugStream Serial

typedef Stream SIM7600StreamType;
typedef const __FlashStringHelper *SIM7600FlashStringPtr;

#define prog_char char PROGMEM

/** Object that controls and keeps state for the SIM7600 module. */
class SIM7600 : public SIM7600StreamType {
public:
  SIM7600(int8_t r);
  bool begin(SIM7600StreamType &port);
  // Stream
  int available(void);
  size_t write(uint8_t x);
  int read(void);
  int peek(void);
  void flush();

  bool setBaudrate(uint16_t baud);

  // RTC
  bool enableRTC(uint8_t mode);
  bool readRTC(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *hr, uint8_t *min, uint8_t *sec);

  // SIM query
  uint8_t unlockSIM(char *pin);
  uint8_t getSIMCCID(char *ccid);
  uint8_t getNetworkStatus(void);
  uint8_t getRSSI(void);

  // IMEI
  uint8_t getIMEI(char *imei);

  // SMS handling
  bool setSMSInterrupt(uint8_t i);
  uint8_t getSMSInterrupt(void);
  int8_t getNumSMS(void);
  bool readSMS(uint8_t message_index, char *smsbuff, uint16_t max, uint16_t *readsize);
  bool sendSMS(char *smsaddr, char *smsmsg);
  bool deleteSMS(uint8_t message_index);
  bool getSMSSender(uint8_t message_index, char *sender, int senderlen);
  bool sendUSSD(char *ussdmsg, char *ussdbuff, uint16_t maxlen, uint16_t *readlen);

  // Time
  bool enableNetworkTimeSync(bool onoff);
  bool enableNTPTimeSync(bool onoff, SIM7600FlashStringPtr ntpserver = 0);
  bool getTime(char *time_buffer, uint16_t maxlen);

  // GPS handling
  bool enableGPS(bool onoff);
  int8_t GPSstatus(void);
  uint8_t getGPS(uint8_t arg, char *buffer, uint8_t maxbuff);
  bool getGPS(float *lat, float *lon, time_t *datetime = 0, float *altitude = 0);
  bool enableGPSNMEA(uint8_t enable_value);

  // Phone calls
  bool callPhone(char *phonenum);
  uint8_t getCallStatus(void);
  bool hangUp(void);
  bool pickUp(void);
  bool callerIdNotification(bool enable, uint8_t interrupt = 0);
  bool incomingCallNumber(char *phonenum);

  // Helper functions to verify responses.
  bool expectReply(SIM7600FlashStringPtr reply, uint16_t timeout = 10000);
  bool sendCheckReply(char *send, char *reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  bool sendCheckReply(SIM7600FlashStringPtr send, SIM7600FlashStringPtr reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  bool sendCheckReply(char *send, SIM7600FlashStringPtr reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);

protected:
  int8_t _rstpin; ///< Reset pin
  char replybuffer[255];  ///< buffer for holding replies from the module
  SIM7600FlashStringPtr ok_reply;    ///< OK reply for successful requests

  void flushInput();
  uint16_t readRaw(uint16_t read_length);
  uint8_t readline(uint16_t timeout = DEFAULT_TIMEOUT_MS, bool multiline = false);
  uint8_t getReply(char *send, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  uint8_t getReply(SIM7600FlashStringPtr send, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  uint8_t getReply(SIM7600FlashStringPtr prefix, char *suffix, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  uint8_t getReply(SIM7600FlashStringPtr prefix, int32_t suffix, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  uint8_t getReply(SIM7600FlashStringPtr prefix, int32_t suffix1, int32_t suffix2, uint16_t timeout); // Don't set default value or else
                                      // function call is ambiguous.
  uint8_t getReplyQuoted(SIM7600FlashStringPtr prefix, SIM7600FlashStringPtr suffix, uint16_t timeout = DEFAULT_TIMEOUT_MS);

  bool sendCheckReply(SIM7600FlashStringPtr prefix, char *suffix, SIM7600FlashStringPtr reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  bool sendCheckReply(SIM7600FlashStringPtr prefix, int32_t suffix, SIM7600FlashStringPtr reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  bool sendCheckReply(SIM7600FlashStringPtr prefix, int32_t suffix, int32_t suffix2, SIM7600FlashStringPtr reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);
  bool sendCheckReplyQuoted(SIM7600FlashStringPtr prefix, SIM7600FlashStringPtr suffix, SIM7600FlashStringPtr reply, uint16_t timeout = DEFAULT_TIMEOUT_MS);

  bool parseReply(SIM7600FlashStringPtr toreply, uint16_t *v, char divider = ',', uint8_t index = 0);
  bool parseReply(SIM7600FlashStringPtr toreply, char *v, char divider = ',', uint8_t index = 0);
  bool parseReply(SIM7600FlashStringPtr toreply, float *f, char divider, uint8_t index);
  bool parseReplyQuoted(SIM7600FlashStringPtr toreply, char *v, int maxlen, char divider, uint8_t index);

  bool sendParseReply(SIM7600FlashStringPtr tosend, SIM7600FlashStringPtr toreply, uint16_t *v, char divider = ',', uint8_t index = 0);
  bool sendParseReply(SIM7600FlashStringPtr tosend, SIM7600FlashStringPtr toreply, float *f, char divider = ',', uint8_t index = 0);

  static bool _incomingCall; ///< Incoming call state var
  static void onIncomingCall();

  SIM7600StreamType *mySerial; ///< Serial connection
};

#endif

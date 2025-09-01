/*!
 * @file SIM7600.cpp
 *
 * @mainpage WaveShare SIM7600
 *
 * @section intro_sec Introduction
 *
 * This is a library for SIM7600 Cellular Module
 *
 * @section author Author
 *
 * @section license License
 *
 * BSD license, all text above must be included in any redistribution
 *
 */

#include "SIM7600.h"

/**
 * @brief Construct a new SIM7600 object
 *
 * @param rst The reset pin
 */
SIM7600::SIM7600(int8_t rst) {
  _rstpin = rst;
  mySerial = 0;
  ok_reply = F("OK");
}

/**
 * @brief Connect to the cell module
 *
 * @param port the serial connection to use to connect
 * @return bool true on success, false if a connection cannot be made
 */
bool SIM7600::begin(Stream &port) {
  mySerial = &port;

  pinMode(_rstpin, OUTPUT);
  digitalWrite(_rstpin, HIGH);
  delay(10);
  digitalWrite(_rstpin, LOW);
  delay(100);
  digitalWrite(_rstpin, HIGH);

  DebugStream.println(F("Attempting to open comm with ATs"));
  // give 7 seconds to reboot
  int16_t timeout = 7000;

  while (timeout > 0) {
    while (mySerial->available())
      mySerial->read();
    if (sendCheckReply(F("AT"), ok_reply))
      break;
    while (mySerial->available())
      mySerial->read();
    if (sendCheckReply(F("AT"), F("AT")))
      break;
    delay(500);
    timeout -= 500;
  }

  if (timeout <= 0) {

    DebugStream.println(F("Timeout: No response to AT... last ditch attempt."));
    sendCheckReply(F("AT"), ok_reply);
    delay(100);
    sendCheckReply(F("AT"), ok_reply);
    delay(100);
    sendCheckReply(F("AT"), ok_reply);
    delay(100);
  }

  // turn off Echo!
  sendCheckReply(F("ATE0"), ok_reply);
  delay(100);

  if (!sendCheckReply(F("ATE0"), ok_reply)) {
    return false;
  }

  // turn on hangupitude
  sendCheckReply(F("AT+CVHU=0"), ok_reply);

  delay(100);
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.println("ATI");

  mySerial->println("ATI");
  readline(500, true);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

#if defined(PREF_SMS_STORAGE)
  sendCheckReply(F("AT+CPMS=" PREF_SMS_STORAGE "," PREF_SMS_STORAGE "," PREF_SMS_STORAGE), ok_reply);
#endif

  return true;
}

/********* Serial port ********************************************/

/**
 * @brief  Set the baud rate that the module will use
 *
 * @param baud The new baud rate to set
 * @return true: success, false: failure
 */
bool SIM7600::setBaudrate(uint16_t baud) {
  return sendCheckReply(F("AT+IPREX="), baud, ok_reply);
}

/********* Real Time Clock ********************************************/
// This RTC setup isn't fully operational
// see https://forums.adafruit.com/viewtopic.php?f=19&t=58002&p=294235#p294235
/**
 * @brief Read the Real Time Clock
 *
 * **NOTE** Currently this function **will only fill the 'year' variable**
 *
 * @param year Pointer to a uint8_t to be set with year data
 * @param month Pointer to a uint8_t to be set with month data **NOT WORKING**
 * @param day Pointer to a uint8_t to be set with day data **NOT WORKING**
 * @param hr Pointer to a uint8_t to be set with hour data **NOT WORKING**
 * @param min Pointer to a uint8_t to be set with minute data **NOT WORKING**
 * @param sec Pointer to a uint8_t to be set with year data **NOT WORKING**
 * @return true: success, false: failure
 */
bool SIM7600::readRTC(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *hr, uint8_t *min, uint8_t *sec) {
  uint16_t v = 0;
  sendParseReply(F("AT+CCLK?"), F("+CCLK: "), &v, '/', 0);

  *year = v;

  // avoid non-used warning
#if 0
  (void *)month;
  (void *)day;
  (void *)hr;
  (void *)min;
  (void *)sec;
#else
  month = month;
  day = day;
  hr = hr;
  min = min;
  sec = sec;
#endif

  DebugStream.println(*year);
  return true;
}

/**
 * @brief Enable the Real Time Clock
 *
 * @param mode  1: Enable 0: Disable
 * @return true: Success , false: failure
 */
bool SIM7600::enableRTC(uint8_t mode) {
  if (!sendCheckReply(F("AT+CLTS="), mode, ok_reply))
    return false;
  return sendCheckReply(F("AT&W"), ok_reply);
}

/********* SIM ***********************************************************/

/**
 * @brief Unlock the sim with a provided PIN
 *
 * @param pin Pointer to a buffer with the PIN to use to unlock the SIM
 * @return uint8_t
 */
uint8_t SIM7600::unlockSIM(char *pin) {
  char sendbuff[14] = "AT+CPIN=";
  sendbuff[8] = pin[0];
  sendbuff[9] = pin[1];
  sendbuff[10] = pin[2];
  sendbuff[11] = pin[3];
  sendbuff[12] = '\0';

  return sendCheckReply(sendbuff, ok_reply);
}

/**
 * @brief Get the SIM CCID
 *
 * @param ccid Pointer to  a buffer to be filled with the CCID
 * @return uint8_t
 */
uint8_t SIM7600::getSIMCCID(char *ccid) {
  getReply(F("AT+CCID"));
  // up to 28 chars for reply, 20 char total ccid
  if (replybuffer[0] == '+') {
    strncpy(ccid, replybuffer + 8, 20);
  } else {
    strncpy(ccid, replybuffer, 20);
  }
  ccid[20] = 0;

  readline(); // eat 'OK'

  return strlen(ccid);
}

/********* IMEI **********************************************************/

/**
 * @brief Get the IMEI
 *
 * @param imei Pointer to a buffer to be filled with the IMEI
 * @return uint8_t
 */
uint8_t SIM7600::getIMEI(char *imei) {
  getReply(F("AT+GSN"));

  // up to 15 chars
  strncpy(imei, replybuffer, 15);
  imei[15] = 0;

  readline(); // eat 'OK'

  return strlen(imei);
}

/********* NETWORK *******************************************************/

/**
 * @brief Gets the current network status
 *
 * @return uint8_t The nework status:
 *
 *  * 0: Not registered, MT is not currently searching a new operator to
 * register to
 *  * 1: Registered, home network
 *  * 2: Not registered, but MT is currently searching a newoperator to register
 * to
 *  * 3: Registration denied
 *  * 4: Unknown
 *  * 5: Registered, roaming
 */
uint8_t SIM7600::getNetworkStatus(void) {
  uint16_t status;

  if (!sendParseReply(F("AT+CREG?"), F("+CREG: "), &status, ',', 1))
    return 0;

  return status;
}
/**
 * @brief get the current Received Signal Strength Indication
 *
 * @return uint8_t The current RSSI
 */
uint8_t SIM7600::getRSSI(void) {
  uint16_t reply;

  if (!sendParseReply(F("AT+CSQ"), F("+CSQ: "), &reply))
    return 0;

  return reply;
}

/********* CALL PHONES **************************************************/

/**
 * @brief Call a phone number
 *
 * @param number Pointer to a buffer with the phone number to call
 * @return true: success, false: failure
 */
bool SIM7600::callPhone(char *number) {
  char sendbuff[35] = "ATD";
  strncpy(sendbuff + 3, number, min(30, (int)strlen(number)));
  uint8_t x = strlen(sendbuff);
  sendbuff[x] = ';';
  sendbuff[x + 1] = 0;
  // DebugStream.println(sendbuff);

  return sendCheckReply(sendbuff, ok_reply);
}

/**
 * @brief Get the current call status
 *
 * @return uint8_t The call status:
 * * 0: Ready
 * * 1: Failure
 * * 2: Unknown
 * * 3: Ringing
 * * 4: Call in progress
 */
uint8_t SIM7600::getCallStatus(void) {
  uint16_t phoneStatus;

  if (!sendParseReply(F("AT+CPAS"), F("+CPAS: "), &phoneStatus))
    return CALL_FAILED; // 1, since 0 is actually a known, good reply

  return phoneStatus;
}
/**
 * @brief End the current call
 *
 * @return true: success, false: failure
 */
bool SIM7600::hangUp(void) { 
  getReply(F("ATH"));

  return (strstr(replybuffer, (prog_char *)F("VOICE CALL: END")) != 0);
}

/**
 * @brief Answer a call
 *
 * @return true: success, false: failure
 */
bool SIM7600::pickUp(void) { 
  return sendCheckReply(F("ATA"), F("VOICE CALL: BEGIN")); 
}

/**
 * @brief On incoming call callback
 *
 */
void SIM7600::onIncomingCall() {

  DebugStream.print(F("> "));
  DebugStream.println(F("Incoming call..."));

  SIM7600::_incomingCall = true;
}

bool SIM7600::_incomingCall = false;

/**
 * @brief Enable or disable caller ID
 *
 * @param enable true to enable, false to disable
 * @param interrupt An optional interrupt to attach
 * @return true: success, false: failure
 */
bool SIM7600::callerIdNotification(bool enable, uint8_t interrupt) {
  if (enable) {
    attachInterrupt(interrupt, onIncomingCall, FALLING);
    return sendCheckReply(F("AT+CLIP=1"), ok_reply);
  }

  detachInterrupt(interrupt);
  return sendCheckReply(F("AT+CLIP=0"), ok_reply);
}

/**
 * @brief Get the number of the incoming call
 *
 * @param phonenum Pointer to a buffer to hold the incoming caller's phone
 * number
 * @return true: success, false: failure
 */
bool SIM7600::incomingCallNumber(char *phonenum) {
  //+CLIP: "<incoming phone number>",145,"",0,"",0
  if (!SIM7600::_incomingCall)
    return false;

  readline();
  while (!strcmp(replybuffer, (prog_char *)F("RING")) == 0) {
    flushInput();
    readline();
  }

  readline(); // reads incoming phone number line

  parseReply(F("+CLIP: \""), phonenum, '"');

  DebugStream.print(F("Phone Number: "));
  DebugStream.println(replybuffer);

  SIM7600::_incomingCall = false;
  return true;
}

/********* SMS **********************************************************/

/**
 * @brief Get the current SMS Iterrupt
 *
 * @return uint8_t The SMS interrupt or 0 on failure
 */
uint8_t SIM7600::getSMSInterrupt(void) {
  uint16_t reply;

  if (!sendParseReply(F("AT+CFGRI?"), F("+CFGRI: "), &reply))
    return 0;

  return reply;
}

/**
 * @brief Attach an interrupt for SMS notifications
 *
 * @param i The interrupt to set
 * @return true: success, false: failure
 */
bool SIM7600::setSMSInterrupt(uint8_t i) {
  return sendCheckReply(F("AT+CFGRI="), i, ok_reply);
}
/**
 * @brief Get the number of SMS
 *
 * @return int8_t The SMS count. -1 on error
 */
int8_t SIM7600::getNumSMS(void) {
  uint16_t numsms;

  // get into text mode
  if (!sendCheckReply(F("AT+CMGF=1"), ok_reply))
    return -1;

  // ask how many sms are stored
  if (sendParseReply(F("AT+CPMS?"), F(PREF_SMS_STORAGE ","), &numsms))
    return numsms;
  if (sendParseReply(F("AT+CPMS?"), F("\"SM\","), &numsms))
    return numsms;
  if (sendParseReply(F("AT+CPMS?"), F("\"SM_P\","), &numsms))
    return numsms;
  return -1;
}

// Reading SMS's is a bit involved so we don't use helpers that may cause delays
// or debug printouts!

/**
 * @brief Read an SMS message into a provided buffer
 *
 * @param message_index The SMS message index to retrieve
 * @param smsbuff SMS message buffer
 * @param maxlen The maximum read length
 * @param readlen The length read
 * @return true: success, false: failure
 */
bool SIM7600::readSMS(uint8_t message_index, char *smsbuff, uint16_t maxlen, uint16_t *readlen) {
  // text mode
  if (!sendCheckReply(F("AT+CMGF=1"), ok_reply))
    return false;

  // show all text mode parameters
  if (!sendCheckReply(F("AT+CSDH=1"), ok_reply))
    return false;

  // parse out the SMS len
  uint16_t thesmslen = 0;

  DebugStream.print(F("AT+CMGR="));
  DebugStream.println(message_index);

  // getReply(F("AT+CMGR="), message_index, 1000);  //  do not print debug!
  mySerial->print(F("AT+CMGR="));
  mySerial->println(message_index);
  readline(1000); // timeout

  // DebugStream.print(F("Reply: ")); DebugStream.println(replybuffer);
  // parse it out...

  DebugStream.println(replybuffer);

  if (!parseReply(F("+CMGR:"), &thesmslen, ',', 11)) {
    *readlen = 0;
    return false;
  }

  readRaw(thesmslen);

  flushInput();

  uint16_t thelen = min(maxlen, (uint16_t)strlen(replybuffer));
  strncpy(smsbuff, replybuffer, thelen);
  smsbuff[thelen] = 0; // end the string

  DebugStream.println(replybuffer);

  *readlen = thelen;
  return true;
}

/**
 * @brief Retrieve the sender of the specified SMS message and copy it as a
 *    string to the sender buffer.  Up to senderlen characters of the sender
 * will be copied and a null terminator will be added if less than senderlen
 *    characters are copied to the result.
 *
 * @param message_index The SMS message index to retrieve the sender for
 * @param sender Pointer to a buffer to fill with the sender
 * @param senderlen The maximum length to read
 * @return true: a result was successfully retrieved, false: failure
 */
bool SIM7600::getSMSSender(uint8_t message_index, char *sender, int senderlen) {
  // Ensure text mode and all text mode parameters are sent.
  if (!sendCheckReply(F("AT+CMGF=1"), ok_reply))
    return false;
  if (!sendCheckReply(F("AT+CSDH=1"), ok_reply))
    return false;

  DebugStream.print(F("AT+CMGR="));
  DebugStream.println(message_index);

  // Send command to retrieve SMS message and parse a line of response.
  mySerial->print(F("AT+CMGR="));
  mySerial->println(message_index);
  readline(1000);

  DebugStream.println(replybuffer);

  // Parse the second field in the response.
  bool result = parseReplyQuoted(F("+CMGR:"), sender, senderlen, ',', 1);
  // Drop any remaining data from the response.
  flushInput();
  return result;
}

/**
 * @brief Send an SMS Message from a buffer provided
 *
 * @param smsaddr The SMS address buffer
 * @param smsmsg The SMS message buffer
 * @return true: success, false: failure
 */
bool SIM7600::sendSMS(char *smsaddr, char *smsmsg) {
  if (!sendCheckReply(F("AT+CMGF=1"), ok_reply))
    return false;

  char sendcmd[30] = "AT+CMGS=\"";
  strncpy(sendcmd + 9, smsaddr,
          30 - 9 - 2); // 9 bytes beginning, 2 bytes for close quote + null
  sendcmd[strlen(sendcmd)] = '\"';

  if (!sendCheckReply(sendcmd, F("> ")))
    return false;

  DebugStream.print(F("> "));
  DebugStream.println(smsmsg);

  mySerial->println(smsmsg);
  mySerial->println();
  mySerial->write(0x1A);

  DebugStream.println("^Z");

  // Eat two sets of CRLF
  readline(200);
  // DebugStream.print("Line 1: "); DebugStream.println(strlen(replybuffer));
  readline(200);
  // DebugStream.print("Line 2: "); DebugStream.println(strlen(replybuffer));
  readline(10000); // read the +CMGS reply, wait up to 10 seconds!!!

  if (strstr(replybuffer, "+CMGS") == 0) {
    return false;
  }
  readline(1000); // read OK

  if (strcmp(replybuffer, "OK") != 0) {
    return false;
  }

  return true;
}

/**
 * @brief Delete an SMS Message
 *
 * @param message_index The message to delete
 * @return true: success, false: failure
 */
bool SIM7600::deleteSMS(uint8_t message_index) {
  if (!sendCheckReply(F("AT+CMGF=1"), ok_reply))
    return false;
  // read an sms
  char sendbuff[12] = "AT+CMGD=000";
  sendbuff[8] = (message_index / 100) + '0';
  message_index %= 100;
  sendbuff[9] = (message_index / 10) + '0';
  message_index %= 10;
  sendbuff[10] = message_index + '0';

  return sendCheckReply(sendbuff, ok_reply, 2000);
}

/********* USSD *********************************************************/

/**
 * @brief Send USSD
 *
 * @param ussdmsg The USSD message buffer
 * @param ussdbuff The USSD bufer
 * @param maxlen The maximum read length
 * @param readlen The length actually read
 * @return true: success, false: failure
 */
bool SIM7600::sendUSSD(char *ussdmsg, char *ussdbuff, uint16_t maxlen, uint16_t *readlen) {
  if (!sendCheckReply(F("AT+CUSD=1"), ok_reply))
    return false;

  char sendcmd[30] = "AT+CUSD=1,\"";
  strncpy(sendcmd + 11, ussdmsg,
          30 - 11 - 2); // 11 bytes beginning, 2 bytes for close quote + null
  sendcmd[strlen(sendcmd)] = '\"';

  if (!sendCheckReply(sendcmd, ok_reply)) {
    *readlen = 0;
    return false;
  } else {
    readline(10000); // read the +CUSD reply, wait up to 10 seconds!!!
    // DebugStream.print("* "); DebugStream.println(replybuffer);
    char *p = strstr(replybuffer, PSTR("+CUSD: "));
    if (p == 0) {
      *readlen = 0;
      return false;
    }
    p += 7; //+CUSD
    // Find " to get start of ussd message.
    p = strchr(p, '\"');
    if (p == 0) {
      *readlen = 0;
      return false;
    }
    p += 1; //"
    // Find " to get end of ussd message.
    char *strend = strchr(p, '\"');

    uint16_t lentocopy = min(maxlen - 1, strend - p);
    strncpy(ussdbuff, p, lentocopy + 1);
    ussdbuff[lentocopy] = 0;
    *readlen = lentocopy;
  }
  return true;
}

/********* TIME **********************************************************/

/**
 * @brief Enable network time sync
 *
 * @param onoff true: enable false: disable
 * @return true: success, false: failure
 */
bool SIM7600::enableNetworkTimeSync(bool onoff) {
  if (onoff) {
    if (!sendCheckReply(F("AT+CLTS=1"), ok_reply))
      return false;
  } else {
    if (!sendCheckReply(F("AT+CLTS=0"), ok_reply))
      return false;
  }

  flushInput(); // eat any 'Unsolicted Result Code'

  return true;
}

/**
 * @brief Enable NTP time sync
 *
 * @param onoff true: enable false: disable
 * @param ntpserver The NTP server buffer
 * @return true: success, false: failure
 */
bool SIM7600::enableNTPTimeSync(bool onoff, SIM7600FlashStringPtr ntpserver) {
  if (onoff) {
    if (!sendCheckReply(F("AT+CNTPCID=1"), ok_reply))
      return false;

    mySerial->print(F("AT+CNTP=\""));
    if (ntpserver != 0) {
      mySerial->print(ntpserver);
    } else {
      mySerial->print(F("pool.ntp.org"));
    }
    mySerial->println(F("\",0"));
    readline(DEFAULT_TIMEOUT_MS);
    if (strcmp(replybuffer, "OK") != 0)
      return false;

    if (!sendCheckReply(F("AT+CNTP"), ok_reply, 10000))
      return false;

    uint16_t status;
    readline(10000);
    if (!parseReply(F("+CNTP:"), &status))
      return false;
  } else {
    if (!sendCheckReply(F("AT+CNTPCID=0"), ok_reply))
      return false;
  }

  return true;
}

/**
 * @brief Get the current time
 *
 * @param time_buffer Pointer to a buffer to hold the time
 * @param maxlen maximum read length
 * @return true: success, false: failure
 */
bool SIM7600::getTime(char *time_buffer, uint16_t maxlen) {
  getReply(F("AT+CCLK?"), (uint16_t)10000);
  if (strncmp(replybuffer, "+CCLK: ", 7) != 0)
    return false;

  char *p = replybuffer + 7;
  uint16_t lentocopy = min(maxlen - 1, (int)strlen(p));
  strncpy(time_buffer, p, lentocopy + 1);
  time_buffer[lentocopy] = 0;

  readline(); // eat OK

  return true;
}

/********* GPS **********************************************************/

/**
 * @brief Enable GPS
 *
 * @param onoff true: enable false: disable
 * @return true: success, false: failure
 */
bool SIM7600::enableGPS(bool onoff) {
  uint16_t state;

  // first check if its already on or off
  if (!SIM7600::sendParseReply(F("AT+CGPS?"), F("+CGPS: "), &state))
    return false;

  if (onoff && !state) {
    if (!sendCheckReply(F("AT+CGPS=1"), ok_reply))
      return false;
  } else if (!onoff && state) {
    if (!sendCheckReply(F("AT+CGPS=0"), ok_reply))
      return false;
    // this takes a little time
    readline(2000); // eat '+CGPS: 0'
  }
  return true;
}

/**
 * @brief Get the GPS status
 *
 * @return int8_t The GPS status:
 * * 0: GPS off
 * 1: No fix
 * 2: 2D fix
 * 3: 3D fix
 */
int8_t SIM7600::GPSstatus(void) {
  getReply(F("AT+CGPSINFO"));
  char *p = strstr(replybuffer, (prog_char *)F("+CGPSINFO:"));
  if (p == 0)
    return -1;
  if (p[10] != ',')
    return 3; // if you get anything, its 3D fix
  return 0;
}

/**
 * @brief Fill a buffer with the current GPS reading
 *
 * @param arg The mode to use, where applicable
 * @param buffer The buffer to be filled with the reading
 * @param maxbuff the maximum amound to write into the buffer
 * @return uint8_t The number of bytes read
 */
uint8_t SIM7600::getGPS(uint8_t arg, char *buffer, uint8_t maxbuff) {
  int32_t x = arg;

  getReply(F("AT+CGPSINFO"));

  char *p = strstr(replybuffer, (prog_char *)F("SINF"));
  if (p == 0) {
    buffer[0] = 0;
    return 0;
  }

  p += 6;

  uint8_t len = max(maxbuff - 1, (int)strlen(p));
  strncpy(buffer, p, len);
  buffer[len] = 0;

  readline(); // eat 'OK'
  return len;
}

/**
 * @brief Get a GPS reading
 *
 * @param lat  Pointer to a buffer to be filled with thelatitude
 * @param lon Pointer to a buffer to be filled with the longitude
 * @param time Pointer to a buffer to be filled with the time
 * kilometers per hour
 * @param date Pointer to a buffer to be filled with the date
 * @param altitude Pointer to a buffer to be filled with the altitude
 * @return true: success, false: failure
 */
bool SIM7600::getGPS(float *lat, float *lon, time_t *datetime, float *altitude) {
  tmElements_t tSet;
  char dd[3], mm[3], yy[3], hh[3], nn[3], ss[3];
  char gpsbuffer[120];

  memset(dd, '\0', 3);              // Initialize the string
  memset(mm, '\0', 3);              // Initialize the string
  memset(yy, '\0', 3);              // Initialize the string
  memset(hh, '\0', 3);              // Initialize the string
  memset(nn, '\0', 3);              // Initialize the string
  memset(ss, '\0', 3);              // Initialize the string

  // we need at least a 2D fix
  if (GPSstatus() < 2)
    return false;

  // grab the mode 2^5 gps csv from the sim808
  uint8_t res_len = getGPS(32, gpsbuffer, 120);

  // make sure we have a response
  if (res_len == 0)
    return false;

    // Parse 3G respose
    // +CGPSINFO:4043.000000,N,07400.000000,W,151015,203802.1,-12.0,0.0,0
    // skip beginning

    // grab the latitude
    char *latp = strtok(gpsbuffer, ",");
    if (!latp)
      return false;

    // grab latitude direction
    char *latdir = strtok(NULL, ",");
    if (!latdir)
      return false;

    // grab longitude
    char *longp = strtok(NULL, ",");
    if (!longp)
      return false;

    // grab longitude direction
    char *longdir = strtok(NULL, ",");
    if (!longdir)
      return false;

    // date
    char *gpsDate = strtok(NULL, ",");
    if (!gpsDate)
      return false;
    // Day
    strncpy(dd, gpsDate, 2);
    dd[2] = '\0';
    tSet.Day = atoi(dd);
    // Month
    strncpy(mm, gpsDate + 2, 2);
    mm[2] = '\0';
    tSet.Month = atoi(mm);
    // Year
    strncpy(yy, gpsDate + 4, 2);
    yy[2] = '\0';
    tSet.Year = atoi(yy);

    // time
    char *gpsTime = strtok(NULL, ",");
    if (!gpsTime)
      return false;
    // Hour
    strncpy(hh, gpsTime, 2);
    hh[2] = '\0';
    tSet.Hour = atoi(hh);
    // Minute
    strncpy(nn, gpsTime + 2, 2);
    nn[2] = '\0';
    tSet.Minute = atoi(nn);
    // Second
    strncpy(ss, gpsTime + 4, 2);
    ss[2] = '\0';
    tSet.Second = atoi(ss);

    *datetime = makeTime(tSet);
    
    // only grab altitude if needed
    if (altitude != NULL) {
      // grab altitude
      char *altp = strtok(NULL, ",");
      if (!altp)
        return false;
      *altitude = atof(altp);
    }

    double latitude = atof(latp);
    double longitude = atof(longp);

    // convert latitude from minutes to decimal
    float degrees = floor(latitude / 100);
    double minutes = latitude - (100 * degrees);
    minutes /= 60;
    degrees += minutes;

    // turn direction into + or -
    if (latdir[0] == 'S')
      degrees *= -1;

    *lat = degrees;

    // convert longitude from minutes to decimal
    degrees = floor(longitude / 100);
    minutes = longitude - (100 * degrees);
    minutes /= 60;
    degrees += minutes;

    // turn direction into + or -
    if (longdir[0] == 'W')
      degrees *= -1;

    *lon = degrees;

  return true;
}

/**
 * @brief Enable GPS NMEA output

 * @param enable_value
 *  * 0: Disable
 *  * 1: Enable
 * * For Others see the application note for information on the
 * `AT+CGPSOUT=` command:
 *
 * @return true: success, false: failure
 */
bool SIM7600::enableGPSNMEA(uint8_t enable_value) {
  char sendbuff[15] = "AT+CGPSOUT=000";
  sendbuff[11] = (enable_value / 100) + '0';
  enable_value %= 100;
  sendbuff[12] = (enable_value / 10) + '0';
  enable_value %= 10;
  sendbuff[13] = enable_value + '0';

  return sendCheckReply(sendbuff, ok_reply, 2000);
}

/********* HELPERS *********************************************/

/**
 * @brief Check if the received reply matches the expectation
 *
 * @param reply The expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::expectReply(SIM7600FlashStringPtr reply, uint16_t timeout) {
  readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

/********* LOW LEVEL *******************************************/

/**
 * @brief Serial data available
 *
 * @return int
 */
inline int SIM7600::available(void) { return mySerial->available(); } ///<
/**
 * @brief Serial write
 *
 * @param x
 * @return size_t
 */
inline size_t SIM7600::write(uint8_t x) {
  return mySerial->write(x);
} ///<
/**
 * @brief Serial read
 *
 * @return int
 */
inline int SIM7600::read(void) { return mySerial->read(); } ///<
/**
 * @brief Serial peek
 *
 * @return int
 */
inline int SIM7600::peek(void) { return mySerial->peek(); } ///<
/**
 * @brief Flush the serial data
 *
 */
inline void SIM7600::flush() { mySerial->flush(); } ///<
/**
 * @brief Read all available serial input to flush pending data.
 *
 */
void SIM7600::flushInput() {
  uint16_t timeoutloop = 0;
  while (timeoutloop++ < 40) {
    while (available()) {
      read();
      timeoutloop = 0; // If char was received reset the timer
    }
    delay(1);
  }
}
/**
 * @brief Read directly into the reply buffer
 *
 * @param read_length The number of bytes to return
 * @return uint16_t The number of bytes read
 */
uint16_t SIM7600::readRaw(uint16_t read_length) {
  uint16_t idx = 0;

  while (read_length && (idx < sizeof(replybuffer) - 1)) {
    if (mySerial->available()) {
      replybuffer[idx] = mySerial->read();
      idx++;
      read_length--;
    }
  }
  replybuffer[idx] = 0;

  return idx;
}

/**
 * @brief Read a single line or up to 254 bytes
 *
 * @param timeout Reply timeout
 * @param multiline true: read the maximum amount. false: read up to the second
 * newline
 * @return uint8_t the number of bytes read
 */
uint8_t SIM7600::readline(uint16_t timeout, bool multiline) {
  uint16_t replyidx = 0;

  while (timeout--) {
    if (replyidx >= 254) {
      // DebugStream.println(F("SPACE"));
      break;
    }

    while (mySerial->available()) {
      char c = mySerial->read();
      if (c == '\r')
        continue;
      if (c == 0xA) {
        if (replyidx == 0) // the first 0x0A is ignored
          continue;

        if (!multiline) {
          timeout = 0; // the second 0x0A is the end of the line
          break;
        }
      }
      replybuffer[replyidx] = c;
      // DebugStream.print(c, HEX); DebugStream.print("#"); DebugStream.println(c);
      replyidx++;
    }

    if (timeout == 0) {
      // DebugStream.println(F("TIMEOUT"));
      break;
    }
    delay(1);
  }
  replybuffer[replyidx] = 0; // null term
  return replyidx;
}
/**
 * @brief Send a command and return the reply
 *
 * @param send The char* command to send
 * @param timeout Timeout for reading a  response
 * @return uint8_t The response length
 */
uint8_t SIM7600::getReply(char *send, uint16_t timeout) {
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.println(send);

  mySerial->println(send);

  uint8_t l = readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return l;
}

/**
 * @brief Send a command and return the reply
 *
 * @param send The SIM7600FlashStringPtr command to send
 * @param timeout Timeout for reading a response
 * @return uint8_t The response length
 */
uint8_t SIM7600::getReply(SIM7600FlashStringPtr send, uint16_t timeout) {
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.println(send);

  mySerial->println(send);

  uint8_t l = readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return l;
}

// Send prefix, suffix, and newline. Return response (and also set replybuffer
// with response).
/**
 * @brief Send a command as prefix and suffix
 *
 * @param prefix Pointer to a buffer with the command prefix
 * @param suffix Pointer to a buffer with the command suffix
 * @param timeout Timeout for reading a response
 * @return uint8_t The response length
 */
uint8_t SIM7600::getReply(SIM7600FlashStringPtr prefix, char *suffix, uint16_t timeout) {
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.print(prefix);
  DebugStream.println(suffix);

  mySerial->print(prefix);
  mySerial->println(suffix);

  uint8_t l = readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return l;
}

// Send prefix, suffix, and newline. Return response (and also set replybuffer
// with response).
/**
 * @brief Send a command with
 *
 * @param prefix Pointer to a buffer with the command prefix
 * @param suffix The command suffix
 * @param timeout Timeout for reading a response
 * @return uint8_t The response length
 */
uint8_t SIM7600::getReply(SIM7600FlashStringPtr prefix, int32_t suffix,
                                uint16_t timeout) {
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.print(prefix);
  DebugStream.println(suffix, DEC);

  mySerial->print(prefix);
  mySerial->println(suffix, DEC);

  uint8_t l = readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return l;
}

// Send prefix, suffix, suffix2, and newline. Return response (and also set
// replybuffer with response).
/**
 * @brief Send command with prefix and two suffixes
 *
 * @param prefix Pointer to a buffer with the command prefix
 * @param suffix1 The comannd first suffix
 * @param suffix2 The command second suffix
 * @param timeout Timeout for reading a response
 * @return uint8_t The response length
 */
uint8_t SIM7600::getReply(SIM7600FlashStringPtr prefix, int32_t suffix1, int32_t suffix2, uint16_t timeout) {
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.print(prefix);
  DebugStream.print(suffix1, DEC);
  DebugStream.print(',');
  DebugStream.println(suffix2, DEC);

  mySerial->print(prefix);
  mySerial->print(suffix1);
  mySerial->print(',');
  mySerial->println(suffix2, DEC);

  uint8_t l = readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return l;
}

// Send prefix, ", suffix, ", and newline. Return response (and also set
// replybuffer with response).
/**
 * @brief Send command prefix and suffix, returning the response length
 *
 * @param prefix Pointer to a buffer with the command prefix
 * @param suffix Pointer to a buffer with the command suffix
 * @param timeout Timeout for reading a response
 * @return uint8_t The response length
 */
uint8_t SIM7600::getReplyQuoted(SIM7600FlashStringPtr prefix, SIM7600FlashStringPtr suffix, uint16_t timeout) {
  flushInput();

  DebugStream.print(F("\t---> "));
  DebugStream.print(prefix);
  DebugStream.print('"');
  DebugStream.print(suffix);
  DebugStream.println('"');

  mySerial->print(prefix);
  mySerial->print('"');
  mySerial->print(suffix);
  mySerial->println('"');

  uint8_t l = readline(timeout);

  DebugStream.print(F("\t<--- "));
  DebugStream.println(replybuffer);

  return l;
}

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param send Pointer to the buffer of data to send
 * @param reply Buffer with the expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReply(char *send, char *reply, uint16_t timeout) {
  if (!getReply(send, timeout))
    return false;
  /*
    for (uint8_t i=0; i<strlen(replybuffer); i++) {
    DebugStream.print(replybuffer[i], HEX); DebugStream.print(" ");
    }
    DebugStream.println();
    for (uint8_t i=0; i<strlen(reply); i++) {
      DebugStream.print(reply[i], HEX); DebugStream.print(" ");
    }
    DebugStream.println();
    */
  return (strcmp(replybuffer, reply) == 0);
}

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param send Pointer to the buffer of data to send
 * @param reply Pointer to a buffer with the expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReply(SIM7600FlashStringPtr send, SIM7600FlashStringPtr reply, uint16_t timeout) {
  if (!getReply(send, timeout))
    return false;

  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param send Pointer to the buffer of data to send
 * @param reply Pointer to a buffer with the expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReply(char *send, SIM7600FlashStringPtr reply, uint16_t timeout) {
  if (!getReply(send, timeout))
    return false;
  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

// Send prefix, suffix, and newline.  Verify response matches reply
// parameter.

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param prefix Pointer to a buffer with the prefix send
 * @param suffix Pointer to a buffer with the suffix send
 * @param reply Pointer to a buffer with the expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReply(SIM7600FlashStringPtr prefix, char *suffix, SIM7600FlashStringPtr reply, uint16_t timeout) {
  getReply(prefix, suffix, timeout);
  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

// Send prefix, suffix, and newline.  Verify response matches reply
// parameter.

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param prefix Pointer to a buffer with the prefix to send
 * @param suffix The suffix to send
 * @param reply Pointer to a buffer with the expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReply(SIM7600FlashStringPtr prefix, int32_t suffix, SIM7600FlashStringPtr reply, uint16_t timeout) {
  getReply(prefix, suffix, timeout);
  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param prefix Ponter to a buffer with the prefix to send
 * @param suffix1 The first suffix to send
 * @param suffix2 The second suffix to send
 * @param reply Pointer to a buffer with the expected reply
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReply(SIM7600FlashStringPtr prefix, int32_t suffix1, int32_t suffix2, SIM7600FlashStringPtr reply, uint16_t timeout) {
  getReply(prefix, suffix1, suffix2, timeout);
  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

/**
 * @brief Send data and verify the response matches an expected response
 *
 * @param prefix Pointer to a buffer with the prefix to send
 * @param suffix Pointer to a buffer with the suffix to send
 * @param reply Pointer to a buffer with the expected response
 * @param timeout Read timeout
 * @return true: success, false: failure
 */
bool SIM7600::sendCheckReplyQuoted(SIM7600FlashStringPtr prefix, SIM7600FlashStringPtr suffix, SIM7600FlashStringPtr reply, uint16_t timeout) {
  getReplyQuoted(prefix, suffix, timeout);
  return (strcmp(replybuffer, (prog_char *)reply) == 0);
}

/**
 * @brief Parse a string in the response fields using a designated separator
 * and copy the value at the specified index in to the supplied buffer.
 *
 * @param toreply Pointer to a buffer with reply with the field being parsed
 * @param v Pointer to a buffer to fill with the the value from the parsed field
 * @param divider The divider character
 * @param index The index of the parsed field to retrieve
 * @return true: success, false: failure
 */
bool SIM7600::parseReply(SIM7600FlashStringPtr toreply, uint16_t *v, char divider, uint8_t index) {
  char *p = strstr(replybuffer, (prog_char *)toreply);
  if (p == 0)
    return false;
  p += strlen((prog_char *)toreply);
  // DebugStream.println(p);
  for (uint8_t i = 0; i < index; i++) {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
    // DebugStream.println(p);
  }
  *v = atoi(p);

  return true;
}

/**

 * @brief Parse a string in the response fields using a designated separator
 * and copy the string at the specified index in to the supplied char buffer.
 *
 * @param toreply Pointer to a buffer with reply with the field being parsed
 * @param v Pointer to a buffer to fill with the string
 * @param divider The divider character
 * @param index The index of the parsed field to retrieve
 * @return true: success, false: failure
 */
bool SIM7600::parseReply(SIM7600FlashStringPtr toreply, char *v, char divider, uint8_t index) {
  uint8_t i = 0;
  char *p = strstr(replybuffer, (prog_char *)toreply);
  if (p == 0)
    return false;
  p += strlen((prog_char *)toreply);

  for (i = 0; i < index; i++) {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
  }

  for (i = 0; i < strlen(p); i++) {
    if (p[i] == divider)
      break;
    v[i] = p[i];
  }

  v[i] = '\0';

  return true;
}

//
/**
 *
 * @brief Parse a string in the response fields using a designated separator
 * and copy the string (without quotes) at the specified index in to the
 * supplied char buffer.
 *
 * @param toreply Pointer to a buffer with reply with the field being parsed
 * @param v Pointer to a buffer to fill with the string. Make sure to supply a
 * buffer large enough to retrieve the expected value
 * @param maxlen The maximum read length
 * @param divider The divider character
 * @param index The index of the parsed field to retrieve
 * @return true: success, false: failure
 */
bool SIM7600::parseReplyQuoted(SIM7600FlashStringPtr toreply, char *v, int maxlen, char divider, uint8_t index) {
  uint8_t i = 0, j;
  // Verify response starts with toreply.
  char *p = strstr(replybuffer, (prog_char *)toreply);
  if (p == 0)
    return false;
  p += strlen((prog_char *)toreply);

  // Find location of desired response field.
  for (i = 0; i < index; i++) {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
  }

  // Copy characters from response field into result string.
  for (i = 0, j = 0; j < maxlen && i < strlen(p); ++i) {
    // Stop if a divier is found.
    if (p[i] == divider)
      break;
    // Skip any quotation marks.
    else if (p[i] == '"')
      continue;
    v[j++] = p[i];
  }

  // Add a null terminator if result string buffer was not filled.
  if (j < maxlen)
    v[j] = '\0';

  return true;
}

/**
 * @brief Send data and parse the reply
 *
 * @param tosend Pointer to the data buffer to send
 * @param toreply Pointer to a buffer with the expected reply string
 * @param v Pointer to a uint16_t buffer to hold the value of the parsed
 * response
 * @param divider The divider character
 * @param index The index of the parsed field to retrieve
 * @return true: success, false: failure
 */
bool SIM7600::sendParseReply(SIM7600FlashStringPtr tosend, SIM7600FlashStringPtr toreply, uint16_t *v, char divider, uint8_t index) {
  getReply(tosend);

  if (!parseReply(toreply, v, divider, index))
    return false;

  readline(); // eat 'OK'

  return true;
}

// needed for CBC and others

/**
 * @brief Send data and parse the reply
 *
 * @param tosend Pointer to he data buffer to send
 * @param toreply The expected reply string
 * @param f Pointer to a float buffer to hold value of the parsed field
 * @param divider The divider character
 * @param index The index of the parsed field to retrieve
 * @return true: success, false: failure
 */
bool SIM7600::sendParseReply(SIM7600FlashStringPtr tosend, SIM7600FlashStringPtr toreply, float *f, char divider, uint8_t index) {
  getReply(tosend);

  if (!parseReply(toreply, f, divider, index))
    return false;

  readline(); // eat 'OK'

  return true;
}

/**
 * @brief Parse a reply
 *
 * @param toreply Pointer to a buffer with the expected reply string
 * @param f Pointer to a float buffer to hold the value of the parsed field
 * @param divider The divider character
 * @param index The index of the parsed field to retrieve
 * @return true: success, false: failure
 */
bool SIM7600::parseReply(SIM7600FlashStringPtr toreply, float *f, char divider, uint8_t index) {
  char *p = strstr(replybuffer, (prog_char *)toreply);
  if (p == 0)
    return false;
  p += strlen((prog_char *)toreply);
  // DebugStream.println(p);
  for (uint8_t i = 0; i < index; i++) {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
    // DebugStream.println(p);
  }
  *f = atof(p);

  return true;
}

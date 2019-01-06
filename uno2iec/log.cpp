#include "log.h"
#include <stddef.h>

#ifndef NO_LOGGING

static const char S_MAIN[] PROGMEM = "MAIN";
static const char S_IEC[] PROGMEM = "IEC";
static const char S_IFACE[] PROGMEM = "IFACE";

const struct {
  const char abbreviated;
  const char *string;
} facilities[] PROGMEM = {FAC_MAIN, S_MAIN, FAC_IEC, S_IEC, FAC_IFACE, S_IFACE};

static const char siwe[] PROGMEM = "SIWE";

void registerFacilities(void) {
  char strBuf[25];
  for (byte i = 0; i < sizeof(facilities) / sizeof(facilities[0]); ++i) {
    sprintf_P(strBuf, (PGM_P)F("!%c"),
              pgm_read_byte(&facilities[i].abbreviated));
    strcat_P(strBuf, (PGM_P)pgm_read_word(&facilities[i].string));
    COMPORT.print(strBuf);
    COMPORT.print('\r');
  }
  COMPORT.flush();
} // registerFacilities

void Log(byte severity, char facility, char *msg) {
  char strBuf[4];
  sprintf_P(strBuf, (PGM_P)F("D%c%c"), pgm_read_byte(siwe + severity),
            facility);
  // NOTE: Queueing possible, polling of queue could be handled (called) from
  // 'loop()'.
  COMPORT.print(strBuf);
  COMPORT.print(msg);
  COMPORT.print('\r');
} // Log

#endif

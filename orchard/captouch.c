#include "ch.h"
#include "hal.h"
#include "i2c.h"
#include "chprintf.h"

#include "orchard.h"
#include "orchard-events.h"

#include "captouch.h"

#include <stdlib.h>

static I2CDriver *driver;
event_source_t captouch_changed;
static uint16_t captouch_state;

static void captouch_set(uint8_t reg, uint8_t val) {

  uint8_t tx[2] = {reg, val};

  i2cMasterTransmitTimeout(driver, touchAddr,
                           tx, sizeof(tx),
                           NULL, 0,
                           TIME_INFINITE);
}

static uint8_t captouch_get(uint8_t reg) {

  uint8_t val;

  i2cMasterTransmitTimeout(driver, touchAddr,
                           &reg, 1,
                           (void *)&val, 1,
                           TIME_INFINITE);
  return val;
}

static uint16_t captouch_read(void) {

  uint16_t val;
  uint8_t reg;

  reg = ELE_TCHL;
  i2cMasterTransmitTimeout(driver, touchAddr,
                           &reg, 1,
                           (void *)&val, 2,
                           TIME_INFINITE);
  return val;
}

// Configure all registers as described in AN3944
static void captouch_config(void) {

  captouch_set(ELE_CFG, 0x00);  // register updates only allowed when in stop mode

  // Section A
  // This group controls filtering when data is > baseline.
  captouch_set(MHD_R, 0x01);
  captouch_set(NHD_R, 0x01);
  captouch_set(NCL_R, 0x03);
  captouch_set(FDL_R, 0x02);

  // Section B
  // This group controls filtering when data is < baseline.
  captouch_set(MHD_F, 0x01);
  captouch_set(NHD_F, 0x01);
  captouch_set(NCL_F, 0x04);
  captouch_set(FDL_F, 0x02);

  // Section C
  // This group sets touch and release thresholds for each electrode
#if KEY_LAYOUT == LAYOUT_BC1
  captouch_set(ELE0_T, 18); // outer electrodes have an offset
  captouch_set(ELE0_R, 25);
#else
  captouch_set(ELE0_T, TOU_THRESH);
  captouch_set(ELE0_R, REL_THRESH);
#endif
  captouch_set(ELE1_T, TOU_THRESH);
  captouch_set(ELE1_R, REL_THRESH);
  captouch_set(ELE2_T, TOU_THRESH);
  captouch_set(ELE2_R, REL_THRESH);
  captouch_set(ELE3_T, TOU_THRESH);
  captouch_set(ELE3_R, REL_THRESH);
  captouch_set(ELE4_T, TOU_THRESH);
  captouch_set(ELE4_R, REL_THRESH);
  captouch_set(ELE5_T, TOU_THRESH);
  captouch_set(ELE5_R, REL_THRESH);
  captouch_set(ELE6_T, TOU_THRESH);
  captouch_set(ELE6_R, REL_THRESH);
  captouch_set(ELE7_T, TOU_THRESH);
  captouch_set(ELE7_R, REL_THRESH);
  captouch_set(ELE8_T, TOU_THRESH);
  captouch_set(ELE8_R, REL_THRESH);
  captouch_set(ELE9_T, TOU_THRESH);
  captouch_set(ELE9_R, REL_THRESH);
  captouch_set(ELE10_T, TOU_THRESH);
  captouch_set(ELE10_R, REL_THRESH);

#if KEY_LAYOUT == LAYOUT_BC1
  captouch_set(ELE11_T, 18);
  captouch_set(ELE11_R, 25);
#else
  captouch_set(ELE11_T, TOU_THRESH);
  captouch_set(ELE11_R, REL_THRESH);
#endif
  // set debounce requirements
  captouch_set(TCH_DBNC, 0x11); 

  // Section D
  // Set the Filter Configuration
  // Set ESI2
  captouch_set(FIL_CFG, 0x24);

  // Section F
  // Enable Auto Config and auto Reconfig
  captouch_set(ATO_CFG_CTL0, 0x0B);

  captouch_set(ATO_CFG_USL, 0xC4);  // USL 196
  captouch_set(ATO_CFG_LSL, 0x7F);  // LSL 127
  captouch_set(ATO_CFG_TGT, 0xB0);  // target level 176

  // Section E
  // Electrode Configuration
  // Enable 6 Electrodes and set to run mode
  // Set ELE_CFG to 0x00 to return to standby mode
  captouch_set(ELE_CFG, 0x0C);  // Enables all 12 Electrodes

}

void captouchFastBaseline(void) {
  uint8_t ret;
  uint16_t electrode;
  uint8_t i, j;

  captouch_set(ELE_CFG, 0x00);  // disable all electrodes

  for( i = 0x4, j = 0x1e; i < 0x1b; i+= 2, j++ ) {
    electrode = 0;
    ret = captouchGet(i);
    electrode = (uint16_t) ret;
    ret = captouchGet(i+1);
    electrode |= (ret << 8);

    captouchSet(j, (uint8_t) (electrode >> 2));
  }
  
  captouch_set(ELE_CFG, 0x0C);  // enable all electrodes
  chThdSleepMilliseconds(50);

  captouch_state = 0x0; // clear captouch state, it'll be all wrong now
}

void captouch_keychange(eventid_t id) {
  (void) id;
  
  uint16_t mask;
  bool changed = false;

  i2cAcquireBus(driver);
  mask = captouch_read();
  i2cReleaseBus(driver);

  if (captouch_state != mask)
    changed = true;
  captouch_state = mask;

  if (changed)
    chEvtBroadcast(&captouch_changed);
}

uint16_t captouchRead(void) {
  return captouch_state;
}

uint16_t captouchDirectRead(void) {
  uint16_t val;
  
  i2cAcquireBus(driver);
  val = captouch_read();
  i2cReleaseBus(driver);

  return val;
}

void captouchStop() {
  i2cAcquireBus(driver);
  captouch_set(ELE_CFG, 0x00);  // disabling all electrodes puts us in stop mode
  i2cReleaseBus(driver);
}

void captouchStart(I2CDriver *i2cp) {
  driver = i2cp;

  i2cAcquireBus(driver);
  captouch_config();
  i2cReleaseBus(driver);

}

static void dump(uint8_t *byte, uint32_t count) {
  uint32_t i;

  for( i = 0; i < count; i++ ) {
    if( (i % 16) == 0 ) {
      chprintf(stream, "\n\r%08x: ", i );
    }
    chprintf(stream, "%02x ", byte[i] );
  }
  chprintf(stream, "\n\r" );
}

void captouchPrint(uint8_t reg) {
  uint8_t val;

  i2cAcquireBus(driver);
  val = captouch_get(reg);
  i2cReleaseBus(driver);
  
  chprintf( stream, "Value at %02x: %02x\n\r", reg, val );
}

uint8_t captouchGet(uint8_t reg) {
  uint8_t val;

  i2cAcquireBus(driver);
  val = captouch_get(reg);
  i2cReleaseBus(driver);
  
  return val;
}

void captouchSet(uint8_t adr, uint8_t dat) {
  //  chprintf( stream, "Writing %02x into %02x\n\r", dat, adr );

  i2cAcquireBus(driver);
  captouch_set(adr, dat);
  i2cReleaseBus(driver);
}

void captouchRecal(void) {
  i2cAcquireBus(driver);
  captouch_set(0x80, 0x63); // resets the chip
  chThdSleepMilliseconds(50);
  captouch_set(0x80, 0x00); // is this necessary??
  chThdSleepMilliseconds(50);

  captouch_config();
  i2cReleaseBus(driver);
}

void captouchDebug(void) {
  uint8_t i;
  uint8_t val[128];

  for( i = 0; i < 128; i++ ) {
    i2cAcquireBus(driver);
    val[i] = captouch_get(i);
    i2cReleaseBus(driver);
  }
  dump( val, 128 );

}

#define CAL_RETRY_LIMIT 5
void captouchCalibrate(void) {
  uint8_t i;

  i = 0;
  while( i < CAL_RETRY_LIMIT ) {
    chprintf(stream, "Attempt %d of %d", i+1, CAL_RETRY_LIMIT);
    captouchFastBaseline();
    chThdSleepMilliseconds(1000);

    i2cAcquireBus(driver);
    captouch_state = captouch_read();
    i2cReleaseBus(driver);

    if( captouch_state == 0x0 )
      break;

    i++;
  }
}


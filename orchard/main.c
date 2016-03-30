/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "i2c.h"
#include "spi.h"
#include "pal.h"
#include "oled.h"

#include "gfx.h"

#include "shell.h"
#include "chprintf.h"

#include "orchard.h"
#include "orchard-shell.h"
#include "orchard-events.h"

#include "epoch.h"
#include "captouch.h"
#include "accel.h"

#include "kl02x.h"
#include <string.h>
#include <stdlib.h>

struct evt_table orchard_app_events;
struct ui_info orchard_ui_info;
event_source_t ui_call;
event_source_t ta_time_event;
event_source_t ta_update_event;
event_source_t captouch_event;
event_source_t accel_event;
event_source_t accel_test_event;
extern event_source_t captouch_changed;

char xp, yp, zp;

DirIntent dir = dirNone;
struct swipe_state {
  int8_t lastpos;
  DirIntent direction_intent;
  uint32_t lasttime;
} swipe_state;
#define DWELL_THRESH  500  // time to spend in one state before direction intent is null

int8_t swipe_remap(uint16_t raw);

uint32_t ta_time = 1451606400;

static const I2CConfig i2c_config = {
  100000
};

static const SPIConfig spi_config = {
  NULL,
  GPIOA,
  5 
};

static void captouch_cb(EXTDriver *extp, expchannel_t channel) {
  (void)extp;
  (void)channel;

  chSysLockFromISR();
  chEvtBroadcastI(&captouch_event);
  chSysUnlockFromISR();
}

static void accel_cb(EXTDriver *extp, expchannel_t channel) {
  (void)extp;
  (void)channel;

  chSysLockFromISR();
  chEvtBroadcastI(&accel_event);
  chSysUnlockFromISR();
}

static const EXTConfig ext_config = {
  {
    {EXT_CH_MODE_FALLING_EDGE | EXT_CH_MODE_AUTOSTART, accel_cb, PORTB, 3},
    {EXT_CH_MODE_FALLING_EDGE | EXT_CH_MODE_AUTOSTART, captouch_cb, PORTB, 13},
  }
};

extern int print_hex(BaseSequentialStream *chp,
                     const void *block, int count, uint32_t start);

static void print_mcu_info(void) {
  uint32_t sdid = SIM->SDID;
  const char *famid[] = {
    "KL0%d (low-end)",
    "KL1%d (basic)",
    "KL2%d (USB)",
    "KL3%d (Segment LCD)",
    "KL4%d (USB and Segment LCD)",
  };
  const uint8_t ram[] = {
    0,
    1,
    2,
    4,
    8,
    16,
    32,
    64,
  };

  const uint8_t pins[] = {
    16,
    24,
    32,
    36,
    48,
    64,
    80,
  };

  if (((sdid >> 20) & 15) != 1) {
    chprintf(stream, "Device is not Kinetis KL-series\r\n");
    return;
  }

  chprintf(stream, famid[(sdid >> 28) & 15], (sdid >> 24) & 15);
  chprintf(stream, " with %d kB of ram detected"
                   " (Rev: %04x  Die: %04x  Pins: %d).\r\n",
                   ram[(sdid >> 16) & 15],
                   (sdid >> 12) & 15,
                   (sdid >> 7) & 31,
                   pins[(sdid >> 0) & 15]);
}

static void ui_call_handler(eventid_t id) {
  (void)id;
  
  coord_t width;
  font_t font;
  
  switch(orchard_ui_info.font_type) {
  case 1:
    font = gdispOpenFont("UI2");
    break;
  case 2:
    font = gdispOpenFont("DejaVuSans16");
    break;
  default:
    font = gdispOpenFont("fixed_5x8");
  }
  
  width = gdispGetWidth();
  gdispClear(Black);
  gdispDrawStringBox(0, 0, width, gdispGetFontMetric(font, fontHeight),
                     orchard_ui_info.str, font, White, justifyLeft);
  gdispFlush();
  gdispCloseFont(font);

  chHeapFree(orchard_ui_info.str);
}

void init_ui_events(void) {
  chEvtObjectInit(&ui_call);
  evtTableHook(orchard_app_events, ui_call, ui_call_handler);
  
  orchard_ui_info.str = NULL;
  orchard_ui_info.font_type = font_small;
}

OSAL_IRQ_HANDLER(VectorB0) {
  OSAL_IRQ_PROLOGUE();
  osalSysLockFromISR();
  LPTMR0->CSR |= LPTMRx_CSR_TCF; // clear TCF
  
  chEvtBroadcastI(&ta_time_event);
  osalSysUnlockFromISR();
  OSAL_IRQ_EPILOGUE();
}

#define TIMESTR_LEN 32
static void update_handler(eventid_t id) {
  (void) id;
  date_time_t dt;

  orchard_ui_info.str = chHeapAlloc(NULL, TIMESTR_LEN);
  orchard_ui_info.font_type = font_large;
  
  epoch_to_date_time(&dt, ta_time >> 5);

  switch( swipe_state.direction_intent ) {
  case dirLeft:
    chsnprintf( orchard_ui_info.str, TIMESTR_LEN, "%02d:%02d:%02d %2d right", dt.hour, dt.minute, dt.second, swipe_remap(captouchRead()) );
    break;
  case dirRight:
    chsnprintf( orchard_ui_info.str, TIMESTR_LEN, "%02d:%02d:%02d %2d left", dt.hour, dt.minute, dt.second, swipe_remap(captouchRead()) );
    break;
  default:
    chsnprintf( orchard_ui_info.str, TIMESTR_LEN, "%02d:%02d:%02d %2d", dt.hour, dt.minute, dt.second, swipe_remap(captouchRead()) );
    break;
  }
  //  chsnprintf( orchard_ui_info.str, TIMESTR_LEN, "%02d:%02d:%02d %d", dt.hour, dt.minute, dt.second, swipe_remap(captouchRead()) );
  
  chEvtBroadcast(&ui_call);
}

void init_update_events(void) {
  chEvtObjectInit(&ta_update_event);
  evtTableHook(orchard_app_events, ta_update_event, update_handler);
}

static void time_handler(eventid_t id) {
  (void)id;

  xp = ' '; yp = ' '; zp = ' ';
  ta_time++;
  chEvtBroadcast(&ta_update_event);
}

void init_time_events(void) {
  SIM->SCGC5 |= SIM_SCGC5_LPTMR;
  
  LPTMR0->CSR = 0; // clear the timer settings, through a soft reset
  
  chEvtObjectInit(&ta_time_event);
  evtTableHook(orchard_app_events, ta_time_event, time_handler);

  // 32kHz ext source / 16384, so 2Hz trigger rate
  LPTMR0->PSR = LPTMRx_PSR_PRESCALE(0x8) |   // set to D for proper time
    // LPTMRx_PSR_PBYP |
    LPTMRx_PSR_PCS(2);

  LPTMR0->CMR = 1; // trigger after one increment
  //LPTMR0->CNR = 0; // start at 0

  LPTMR0->CSR =
    LPTMRx_CSR_TCF |
    LPTMRx_CSR_TIE |  // enable interrupts
    //    LPTRMx_CSR_TFC |   // CNR reset whenever TCF is set (0)
    //    LMPTRx_CSR_TMS |   // put into time counter mode (0)
    LPTMRx_CSR_TEN;
  
  nvicEnableVector(LPTMR0_IRQn, KINETIS_LPTMR0_PRIORITY);

}

static void accel_pulse_handler(eventid_t id) {
  (void) id;

  if(pulse_axis & PULSE_AXIS_X) {
    //chprintf(stream, "x");
    xp = 'x';
  }
  if( pulse_axis & PULSE_AXIS_Y) {
    //chprintf(stream, "y");
    yp = 'y';
  }
  if( pulse_axis & PULSE_AXIS_Z) {
    //chprintf(stream, "z");
    zp = 'z';
  }
  chEvtBroadcast(&ta_update_event);
}

// 1 is furthest right, and goes up from there to most left
int8_t swipe_remap(uint16_t raw) {
  switch(raw) {
  case 0x0:
    return 0;
  case 0x08:  // TPAD8 -> ELE3 = 1000
    return 1;
  case 0x08 | 0x10:
    return 2;
  case 0x10: // TPAD7 -> ELE4 = 1 0000
    return 3;
  case (0x10 | 0x20):
    return 4;
  case 0x20: // TPAD6 -> ELE 5 = 10 0000
    return 5;
  case (0x40 | 0x20):
    return 6;
  case 0x40: // TPAD5 -> ELE 6 = 100 0000
    return 7;
  case (0x40 | 0x400):
    return 8;
  case 0x400: // TPAD1 -> ELE10 = 100 0000 0000
    return 9;
  case (0x400 | 0x200):
    return 10;
  case 0x200:  // TPAD2 -> ELE9 = 10 0000 0000
    return 11;
  case (0x200 | 0x100):
    return 12;
  case 0x100:  // TPAD3 -> ELE8 = 1 0000 0000
    return 13;
  case (0x100 | 0x80):
    return 14;
  case 0x80:  // TPAD4 -> ELE7 = 1000 0000
    return 15;
  default:
    return -1;
  }
}

void captouch_handler(eventid_t id) {
  (void) id;
  uint32_t curtime = chVTGetSystemTime();
  int8_t curpos = swipe_remap(captouchRead());
  int8_t posdif;

  dir = swipe_state.direction_intent;
  
  if( curpos == 0 ) {
    swipe_state.lastpos = -1;
    swipe_state.lasttime = curtime;
    swipe_state.direction_intent = dirNone;
    return;
  }
  if( curpos == -1 ) {
    swipe_state.lasttime = curtime;
    //swipe_state.direction_intent = dirNone;
    return;
  }
  if( swipe_state.lastpos == -1 ) {
    // starting from untouched state
    swipe_state.lastpos = curpos;
    swipe_state.lasttime = curtime;
    swipe_state.direction_intent = dirNone;
    return;
  }

  posdif = curpos - swipe_state.lastpos;
  if( abs(posdif) <= 1 ) {
    if( curtime - swipe_state.lasttime > DWELL_THRESH ) {
      swipe_state.direction_intent = dirNone;
      chEvtBroadcast(&ta_update_event);
    }
    return;
  } else {
    if( posdif > 0 ) {
      swipe_state.direction_intent = dirLeft;
      swipe_state.lastpos = curpos;
      swipe_state.lasttime = curtime;
      chEvtBroadcast(&ta_update_event);
      return;
    } else {
      swipe_state.direction_intent = dirRight;
      swipe_state.lastpos = curpos;
      swipe_state.lasttime = curtime;
      chEvtBroadcast(&ta_update_event);
      return;
    }
  }
  
}

/*
 * Application entry point.
 */
int main(void)
{
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  i2cStart(i2cDriver, &i2c_config);
  
  spiObjectInit(&SPID1);
  spiStart(&SPID1, &spi_config); 

  GPIOA->PDOR |= (1 << 12); // turn on boost regulator
  //palWritePad(GPIOB, 4, PAL_HIGH);  // turn on boost regulator

  oledStart(&SPID1);
  
  gfxInit();
  
  orchardShellInit();

  chprintf(stream, "\r\n\r\nOrchard shell.  Based on build %s\r\n", gitversion);
  print_mcu_info();

  orchardShellRestart();

  oledOrchardBanner();

  evtTableInit(orchard_app_events, 9);
  
  init_ui_events();
  init_update_events();
  init_time_events();  // enables interrupts from LPTMR

  chEvtObjectInit(&captouch_changed);

  chEvtObjectInit(&captouch_event);
  evtTableHook(orchard_app_events, captouch_event, captouch_keychange);
  evtTableHook(orchard_app_events, captouch_changed, captouch_handler);
  captouchStart(i2cDriver);

  chEvtObjectInit(&accel_event);
  evtTableHook(orchard_app_events, accel_event, accel_irq);
  accelStart(i2cDriver);

  chEvtObjectInit(&accel_test_event);
  evtTableHook(orchard_app_events, accel_test_event, accel_test);

  evtTableHook(orchard_app_events, accel_pulse, accel_pulse_handler);
  xp = ' '; yp = ' '; zp = ' ';
  
  extStart(&EXTD1, &ext_config); // enables interrupts on gpios

  chThdSleepMilliseconds(1000);
  captouchFastBaseline();
  //  captouchCalibrate();
  
  while (TRUE) {
    //if(!palReadPad(GPIOB, 13)) {
      //      chEvtBroadcast(&captouch_event);
    captouch_keychange(0);
      //    }
    chEvtDispatch(evtHandlers(orchard_app_events), chEvtWaitOne(ALL_EVENTS));
  }

}


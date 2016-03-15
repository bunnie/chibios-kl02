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

#include "kl02x.h"
#include <string.h>

struct evt_table orchard_app_events;
struct ui_info orchard_ui_info;
event_source_t ui_call;
event_source_t ta_time_event;
event_source_t ta_update_event;
event_source_t captouch_event;
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

static const EXTConfig ext_config = {
  {
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
                     orchard_ui_info.str, font, White, justifyCenter);
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
  
  epoch_to_date_time(&dt, ta_time);
  
  chsnprintf( orchard_ui_info.str, TIMESTR_LEN, "%02d:%02d:%02d", dt.hour, dt.minute, dt.second );
  chEvtBroadcast(&ui_call);
}

void init_update_events(void) {
  chEvtObjectInit(&ta_update_event);
  evtTableHook(orchard_app_events, ta_update_event, update_handler);
}

static void time_handler(eventid_t id) {
  (void)id;

  ta_time++;
  chEvtBroadcast(&ta_update_event);
}

void init_time_events(void) {
  SIM->SCGC5 |= SIM_SCGC5_LPTMR;
  
  LPTMR0->CSR = 0; // clear the timer settings, through a soft reset
  
  chEvtObjectInit(&ta_time_event);
  evtTableHook(orchard_app_events, ta_time_event, time_handler);

  // 32kHz ext source / 16384, so 2Hz trigger rate
  LPTMR0->PSR = LPTMRx_PSR_PRESCALE(0xD) |
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

  evtTableInit(orchard_app_events, 8);
  
  init_ui_events();
  init_update_events();
  init_time_events();  // enables interrupts from LPTMR

  chEvtObjectInit(&captouch_event);
  evtTableHook(orchard_app_events, captouch_event, captouch_keychange);
  captouchStart(i2cDriver);

  extStart(&EXTD1, &ext_config); // enables interrupts on gpios
  
  while (TRUE) {
    chEvtDispatch(evtHandlers(orchard_app_events), chEvtWaitOne(ALL_EVENTS));
  }

}


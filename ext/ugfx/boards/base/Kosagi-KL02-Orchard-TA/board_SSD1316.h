/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

#include "orchard.h"
#include "oled.h"
//#define SSD1306_PAGE_PREFIX		0x40

#define OLED_CMD 0
#define OLED_DAT 1

#define OLED_MOSI_BIT   7
#define OLED_CLK_BIT    0
#define OLED_RESET_MASK (1 << 6)  // GPIOA
#define OLED_CLK_MASK   (1 << OLED_CLK_BIT)  // GPIOB
#define OLED_MOSI_MASK  (1 << OLED_MOSI_BIT)  // GPIOA


static inline void init_board(GDisplay *g) {
  (void) g;
  GPIOB->PDDR |= OLED_CLK_MASK; // setup for output
  GPIOB->PSOR = OLED_CLK_MASK;   // make sure clock is high for glitchless handover

  GPIOA->PDDR |= OLED_MOSI_MASK; // setup for output
}

static inline void post_init_board(GDisplay *g) {
	(void) g;
}

static inline void setpin_reset(GDisplay *g, bool_t state) {
	(void) g;

	if(state) {
	  // reset LCD
	  GPIOA->PCOR = OLED_RESET_MASK; // active low
	  //	  gpioxSetPadMode(GPIOX, oledResPad, GPIOX_OUT_PUSHPULL | GPIOX_VAL_LOW);
	} else {
	  GPIOA->PSOR = OLED_RESET_MASK;
	  //	  gpioxSetPadMode(GPIOX, oledResPad, GPIOX_OUT_PUSHPULL | GPIOX_VAL_HIGH);
	}
}

static inline void acquire_bus(GDisplay *g) {
	(void) g;

	oledAcquireBus();
}

static inline void release_bus(GDisplay *g) {
	(void) g;

	oledReleaseBus();
}

static inline void write_cmd(GDisplay *g, uint8_t cmd) {
	(void) g;

	oledCmd(cmd);
}

static inline void write_data(GDisplay *g, uint8_t* data, uint16_t length) {
	(void) g;

	oledData(data, length);
}

#endif /* _GDISP_LLD_BOARD_H */

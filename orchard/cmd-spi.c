/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

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
#include "spi.h"

#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"

#define OLED_CMD 0
#define OLED_DAT 1

#define OLED_MOSI_BIT   7
#define OLED_CLK_BIT    0
#define OLED_RESET_MASK (1 << 6)  // GPIOA
#define OLED_CLK_MASK   (1 << OLED_CLK_BIT)  // GPIOB
#define OLED_MOSI_MASK  (1 << OLED_MOSI_BIT)  // GPIOA

#define PORTA_PCR7 *((volatile uint32_t *) 0x4004901c)
#define PORTB_PCR0 *((volatile uint32_t *) 0x4004A000)
#define GPIOB_PTOR *((volatile uint32_t *) 0x400ff04c)
#define GPIOB_PDOR *((volatile uint32_t *) 0x400ff040)
#define GPIOB_PDDR *((volatile uint32_t *) 0x400ff054)
#define GPIOA_PDDR *((volatile uint32_t *) 0x400ff014)

#define PORTA_VOL ((PORT_TypeDef volatile *) PORTA_BASE)
#define PORTB_VOL ((PORT_TypeDef volatile *) PORTB_BASE)

#define GPIOA_VOL ((GPIO_TypeDef volatile *) GPIOA_BASE)
#define GPIOB_VOL ((GPIO_TypeDef volatile *) GPIOB_BASE)

static void spi_CD_setup(void) {
  // hacky setup of all GPIO bits to prep for whacking an extra CD bit in
  // on an 8-bit SPI hardware unit...
  GPIOB->PDDR |= OLED_CLK_MASK; // setup for output
  GPIOB->PSOR = OLED_CLK_MASK;   // make sure clock is high for glitchless handover

  GPIOA->PDDR |= OLED_MOSI_MASK; // setup for output
}

// function only to be called inside of a selected SPI bus, before data xfer phase
//void __attribute__((optimize("O0"))) spi_CD(uint8_t cd) {
static void spi_CD(uint8_t cd) {
  // set the internal data bit
  if( cd )
    GPIOA_VOL->PSOR = OLED_MOSI_MASK;
  else
    GPIOA_VOL->PCOR = OLED_MOSI_MASK;

  // override MOSI mapping
  // the proper way would be to read out the PORTA setting, mask out the mux
  // store the original value, then set it, and then restore it...
  // fuck all that, mel shot first! we care about speed in this routine.
  PORTA_VOL->PCR[OLED_MOSI_BIT] = 0x101;  // 0x301

  // override CLK mapping -- assume it starts high from spi_CD_setup()
  PORTB_VOL->PCR[OLED_CLK_BIT] = 0x101;

  // toggle it low
  GPIOB_VOL->PTOR = OLED_CLK_MASK;

  // revert high using a slower routine -- we need it to stay low for 4 cycles
  //  GPIOB_VOL->PDOR |= OLED_CLK_MASK;
  GPIOB_VOL->PTOR = OLED_CLK_MASK;

  // revert mappings
  PORTB_VOL->PCR[OLED_CLK_BIT] = 0x301;
  PORTA_VOL->PCR[OLED_MOSI_BIT] = 0x301;
}

void OLED_reset(void) {  // pulse the OLED reset line
  GPIOA->PCOR = OLED_RESET_MASK; // active low
  //  usleep(20); // <-- this is what we really want
  chThdSleepMilliseconds(1);   // this is what is convenient
  GPIOA->PSOR = OLED_RESET_MASK;
  chThdSleepMilliseconds(1);   // this is what is convenient
}

void OLED_dat(uint8_t dat) {
  spiSelect(&SPID1);
  spi_CD(OLED_DAT);
  spiStartSend(&SPID1, 1, &dat);
  spiUnselect(&SPID1);
}

void OLED_cmd(uint8_t cmd) {
  spiSelect(&SPID1);
  spi_CD(OLED_CMD);
  spiStartSend(&SPID1, 1, &cmd);
  spiUnselect(&SPID1);
}

void OLED_cmd_pair(uint8_t cmd1, uint8_t cmd2) {
  
  spiSelect(&SPID1);
  spi_CD(OLED_CMD);
  spiStartSend(&SPID1, 1, &cmd1);
  spiUnselect(&SPID1);
  
  spiSelect(&SPID1);
  spi_CD(OLED_CMD);
  spiStartSend(&SPID1, 1, &cmd2);
  spiUnselect(&SPID1);
}


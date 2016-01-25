#include "ch.h"
#include "hal.h"
#include "oled.h"
#include "spi.h"

#include "orchard.h"

#include "gfx.h"


static SPIDriver *driver;

#define OLED_CMD 0
#define OLED_DAT 1

#define OLED_MOSI_BIT   7
#define OLED_CLK_BIT    0
#define OLED_RESET_MASK (1 << 6)  // GPIOA
#define OLED_CLK_MASK   (1 << OLED_CLK_BIT)  // GPIOB
#define OLED_MOSI_MASK  (1 << OLED_MOSI_BIT)  // GPIOA

// not sure the volatile redefines are absolutely needed but I saw some weird behavior
#define PORTA_VOL ((PORT_TypeDef volatile *) PORTA_BASE)
#define PORTB_VOL ((PORT_TypeDef volatile *) PORTB_BASE)

#define GPIOA_VOL ((GPIO_TypeDef volatile *) GPIOA_BASE)
#define GPIOB_VOL ((GPIO_TypeDef volatile *) GPIOB_BASE)

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

static void oled_select(void) {
  spiSelect(driver);
}

static void oled_unselect(void) {
  spiUnselect(driver);
}

void oledStop(SPIDriver *spip) {
  (void)spip;

  oledAcquireBus();
  oledCmd(0xAE); // display off
  oledReleaseBus();

  // or just yank the reset line low?
  // gpioxSetPadMode(GPIOX, oledResPad, GPIOX_OUT_PUSHPULL | GPIOX_VAL_LOW);
  
  // wait 100ms per datasheet
  chThdSleepMilliseconds(100);
}

void oledStart(SPIDriver *spip) {

  driver = spip;

  // setup CD hack
  GPIOB->PDDR |= OLED_CLK_MASK; // setup for output
  GPIOB->PSOR = OLED_CLK_MASK;   // make sure clock is high for glitchless handover

  GPIOA->PDDR |= OLED_MOSI_MASK; // setup for output
}

/* Exported functions, called by uGFX.*/
void oledCmd(uint8_t cmd) {

  spi_CD(OLED_CMD);
  spiStartSend(driver, 1, &cmd);
}

void oledData(uint8_t *data, uint16_t length) {
  unsigned int i;

  for( i = 0; i < length; i++ ) {
    spi_CD(OLED_DAT);
    spiStartSend(driver, 1, &data[i]);
  }
}

void oledAcquireBus(void) {
  spiAcquireBus(driver);
  oled_select();
}

void oledReleaseBus(void) {
  oled_unselect();
  spiReleaseBus(driver);
}

void oledOrchardBanner(void) {
  coord_t width;
  font_t font;
  
  width = gdispGetWidth();
  font = gdispOpenFont("fixed_5x8");
  //  font = gdispOpenFont("UI2");
  //  font = gdispOpenFont("DejaVuSans16");
  
  gdispClear(Black);
  gdispDrawStringBox(0, 0, width, gdispGetFontMetric(font, fontHeight),
                     "Orchard TA build:", font, White, justifyCenter);
  gdispDrawStringBox(0, gdispGetFontMetric(font, fontHeight), width,
		     gdispGetFontMetric(font, fontHeight),
                     &(gitversion[16]), font, White, justifyCenter);
  gdispFlush();
  gdispCloseFont(font);
}


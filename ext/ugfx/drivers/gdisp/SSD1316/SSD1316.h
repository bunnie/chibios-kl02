/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://chibios-gfx.com/license.html
 */

#ifndef _SSD1316_H
#define _SSD1316_H

#define SSD1316_SETCONTRAST 			0x81
#define SSD1316_DISPLAYALLON_RESUME 	0xA4
#define SSD1316_DISPLAYALLON 			0xA5
#define SSD1316_NORMALDISPLAY 			0xA6
#define SSD1316_INVERTDISPLAY 			0xA7
#define SSD1316_DISPLAYOFF 				0xAE
#define SSD1316_DISPLAYON 				0xAF

#define SSD1316_SETDISPLAYOFFSET 		0xD3
#define SSD1316_SETCOMPINS 				0xDA

#define SSD1316_SETVCOMDETECT 			0xDB

#define SSD1316_SETDISPLAYCLOCKDIV 		0xD5
#define SSD1316_SETPRECHARGE 			0xD9
#define SSD1316_ENABLE_CHARGE_PUMP		0x8D

#define SSD1316_SETMULTIPLEX 			0xA8
#define SSD1316_SETSTARTLINE 			0x40

#define SSD1316_MEMORYMODE 				0x20
#define SSD1316_HV_COLUMN_ADDRESS		0x21
#define SSD1316_HV_PAGE_ADDRESS			0x22
#define SSD1316_PAM_PAGE_START			0xB0

#define SSD1316_ROWSCANINC 				0xC0
#define SSD1316_ROWSCANDEC 				0xC8

#define SSD1316_COLSCANINC 				0xA0
#define SSD1316_COLSCANDEC 				0xA1

#define SSD1316_CHARGEPUMP 				0x8D

//#define SSD1316_EXTERNALVCC 			0x1
//#define SSD1316_SWITCHCAPVCC 			0x2

#define SSD1316_SELECT_VCOM_IREF                0xAD
#define SSD1316_AD_INT_IREF                        0x10
#define SSD1316_AD_EXT_VCOM                        0x02


// Scrolling #defines
#define SSD1316_SCROLL_ACTIVATE 						0x2F
#define SSD1316_SCROLL_DEACTIVATE 						0x2E
#define SSD1316_SCROLL_SET_VERTICAL_SCROLL_AREA 		0xA3
#define SSD1316_SCROLL_HORIZONTAL_RIGHT 				0x26
#define SSD1316_SCROLL_HORIZONTAL_LEFT 					0x27
#define SSD1316_SCROLL_VERTICAL_AND_HORIZONTAL_RIGHT 	0x29
#define SSD1316_SCROLL_VERTICAL_AND_HORIZONTAL_LEFT		0x2A

#endif /* _SSD1316_H */

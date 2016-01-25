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

#include <stdlib.h>
#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"

#include "gfx.h"

void cmd_print(BaseSequentialStream *chp, int argc, char *argv[])
{
  coord_t width;
  font_t font;
  uint8_t font_type = 0;

  (void)argv;
  if (argc != 1 && argc != 2) {
    chprintf(chp, "Usage: print <string> [font]\r\n");
    return;
  }
  if( argc == 2 )
    font_type = (uint8_t) strtoul(argv[1], NULL, 10);

  switch(font_type) {
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
                     argv[0], font, White, justifyCenter);
  gdispFlush();
  gdispCloseFont(font);
  
}

orchard_command("print", cmd_print);

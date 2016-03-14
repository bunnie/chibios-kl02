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
#include <string.h>

#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-events.h"

#include "gfx.h"

extern struct ui_info orchard_ui_info;
extern event_source_t ui_call;
extern uint32_t ta_time;

void cmd_print(BaseSequentialStream *chp, int argc, char *argv[])
{
  uint8_t font_type = 0;

  (void)argv;
  if (argc != 1 && argc != 2) {
    chprintf(chp, "Usage: print <string> [font]\r\n");
    return;
  }
  if( argc == 2 )
    font_type = (uint8_t) strtoul(argv[1], NULL, 10);

  orchard_ui_info.font_type = font_type;
  orchard_ui_info.str = chHeapAlloc(NULL, strlen(argv[0]));
  if( orchard_ui_info.str != NULL ) {
    strcpy(orchard_ui_info.str, argv[0]);
  }

  chEvtBroadcast(&ui_call);

}

orchard_command("print", cmd_print);

void cmd_epoch(BaseSequentialStream *chp, int argc, char *argv[])
{
  if( argc != 1 )
    chprintf(chp, "Usage: epoch <time in epoch format>\r\n");

  ta_time = (uint32_t) strtoul(argv[0], NULL, 10);
  
}

orchard_command("epoch", cmd_epoch);

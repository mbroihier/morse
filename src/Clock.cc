
/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

Raspberry Pi Broadcom Clock HW Control

Mark Broihier 2021
*/

#include "../include/Clock.h"

void Clock::initClock() {

  // PLLC is going to be used to drive RF signal.  Since other things are using PLLC,  those things need to use
  // other clock sources that are stable.

  // Switch core clock over to PLLA
  clkReg[CORECLK].div = BCM_PASSWD | CLK_DIV_DIVI(4);
  usleep(100);
  clkReg[CORECLK].ctrl = BCM_PASSWD | CLK_CTL_ENAB | CLK_CTL_SRC(CLK_CTL_SRC_PLLA);

  // Switch EMMC to PLLD
  
  // kill the clock if busy
  if (clkReg[EMMCCLK].ctrl & CLK_CTL_BUSY) {
    do {
      clkReg[EMMCCLK].ctrl = BCM_PASSWD | CLK_CTL_KILL;
    } while (clkReg[EMMCCLK].ctrl & CLK_CTL_BUSY);
  }

  // Set clock source to plld
  clkReg[EMMCCLK].ctrl = BCM_PASSWD | CLK_CTL_SRC(CLK_CTL_SRC_PLLD);
  usleep(10);

  // Enable the EMMC clock
  clkReg[EMMCCLK].ctrl |= BCM_PASSWD | CLK_CTL_ENAB;
  fprintf(stderr, "Clock initialization complete\n");

  // set GP0 Clock to PLLC
  clkReg[GP0CLK].ctrl = BCM_PASSWD | CLK_CTL_SRC(CLK_CTL_SRC_PLLC) | CLK_CTL_ENAB;

  clkReg[CM_PLLC].ctrl = BCM_PASSWD | 0x22A;  // enable PLLC_PER
  usleep(100);

  clkReg[PLLC_CORE].ctrl = BCM_PASSWD | (1 << 8);  // disable what?
  clkReg[PLLC_PER].ctrl = BCM_PASSWD | (1 << 0);  // divisor 1 for max frequency 
  // get frequency of PLLC
  uint32_t pllCtl;
  uint32_t pllFrac;
  uint32_t pllPer;
  pllCtl = clkReg[PLLC_CTRL].ctrl;
  pllFrac = clkReg[PLLC_FRAC].ctrl;
  pllPer = clkReg[PLLC_PER].ctrl;
  frequency = ((XOSC_FREQUENCY * ((uint64_t)pllCtl & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)pllFrac) / (1 << 20)) /
               (2 * pllPer >> 1)) / ((pllCtl >> 12) & 0x7) * 2;
  fprintf(stderr, "PLL C frequency %lu\n", frequency);
  pllcFrequency = frequency;
  // find divider for PLL C clock
  uint32_t divider = 0;
  for (divider = 4095; divider > 1; divider--) {
    //if (centerFrequency * divider > frequency) continue;
    if ((uint64_t)centerFrequency * divider < 200e6) {
      fprintf(stderr, "divider shouldn't get this small - %d\n", divider);
      continue;
    }
    if ((uint64_t)centerFrequency * divider > 1500e6) {
      //fprintf(stderr, "divider still too big, keep going - %d\n", divider);
      continue;
    }
    break;
  }
  fprintf(stderr, "PLL C divider will be %d for center frequency of %d\n", divider, centerFrequency);
  if (divider == 0) {
    fprintf(stderr, "Couldn't find an acceptable divider\n");
    exit(-1);
  }
  clkReg[GP0CLK].div = BCM_PASSWD | CLK_DIV_DIVI(divider);

  // now lets program the frequency for PLLC
  double multiplier = ((double) centerFrequency * divider) / (double) XOSC_FREQUENCY;
  uint32_t scaledMultiplier = multiplier * (double)(1 << 20);
  uint32_t integerPortion = scaledMultiplier >> 20;
  uint32_t fractionalPortion = scaledMultiplier & 0xfffff;
  clkReg[PLLC_CTRL].ctrl = BCM_PASSWD | integerPortion | (0x21 << 12);  // not sure what the 21 is
  usleep(100);
  clkReg[PLLC_FRAC].ctrl = BCM_PASSWD | fractionalPortion;

  pllCtl = clkReg[PLLC_CTRL].ctrl;
  pllFrac = clkReg[PLLC_FRAC].ctrl;
  pllPer = clkReg[PLLC_PER].ctrl;
  frequency = ((XOSC_FREQUENCY * ((uint64_t)pllCtl & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)pllFrac) / (1 << 20)) /
               (2 * pllPer >> 1)) / ((pllCtl >> 12) & 0x7) * 2;
  fprintf(stderr, "PLL C frequency should now be %lu\n", frequency);
  fprintf(stderr, "Multiplier should be %f\n", (double)scaledMultiplier/double(1<<20));
  pllcFrequency = frequency;
  
  // now lets set the PCM clock control
  // kill the clock if busy
  if (clkReg[PCMCLK].ctrl & CLK_CTL_BUSY) {
    do {
      clkReg[PCMCLK].ctrl = BCM_PASSWD | CLK_CTL_KILL;
    } while (clkReg[PCMCLK].ctrl & CLK_CTL_BUSY);
  }
  fprintf(stderr, "PCM clock stopped, changing source to PLLD\n");
  // set PCM Clock to PLLD
  clkReg[PCMCLK].ctrl = BCM_PASSWD | CLK_CTL_SRC(CLK_CTL_SRC_PLLD) | CLK_CTL_ENAB;
  // get frequency of PLLD
  pllCtl = clkReg[PLLD_CTRL].ctrl;
  pllFrac = clkReg[PLLD_FRAC].ctrl;
  pllPer = clkReg[PLLD_PER].ctrl;
  frequency = ((XOSC_FREQUENCY * ((uint64_t)pllCtl & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)pllFrac) / (1 << 20)) /
               (2 * pllPer >> 1)) / ((pllCtl >> 12) & 0x7) * 2;
  fprintf(stderr, "PLL D frequency %lu\n", frequency);
  plldFrequency = frequency;
  sleep(1.0);
  // check for frequency lock
  if (clkReg[CM_LOCK].ctrl & CM_LOCK_FLOCKC > 0) {
    fprintf(stderr, "PLLC clock has locked into its frequency of %lu Hz.\n", pllcFrequency);
  } else {
    fprintf(stderr, "PLLC clock has failed to lock into its frequency of %lu Hz.\n", pllcFrequency);
  }
  if (clkReg[CM_LOCK].ctrl & CM_LOCK_FLOCKD > 0) {
    fprintf(stderr, "PLLD clock has locked into its frequency of %lu Hz.\n", plldFrequency);
  } else {
    fprintf(stderr, "PLLD clock has failed to lock into its frequency of %lu Hz.\n", plldFrequency);
  }

}

Clock::Clock(uint32_t centerFrequency, GPIO * gpio, Peripheral * peripheralUtil) {  // may not need gpio object
  uint8_t *cmBasePtr = reinterpret_cast<uint8_t *>(peripheralUtil->mapPeripheralToUserSpace(CM_BASE, CM_LEN));
  clkReg = reinterpret_cast<CLKCtrlReg *>(cmBasePtr);
  this->gpio = gpio;  // may not need this
  this->centerFrequency = centerFrequency;
  initClock();
}

Clock::~Clock() {
  // before shutdown - look at lock
  fprintf(stderr, "Clock shutting down\n");
  if (clkReg[CM_LOCK].ctrl & CM_LOCK_FLOCKC > 0) {
    fprintf(stderr, "PLLC clock has locked into its frequency of %lu Hz.\n", pllcFrequency);
  } else {
    fprintf(stderr, "PLLC clock has failed to lock into its frequency of %lu Hz.\n", pllcFrequency);
  }
  if (clkReg[CM_LOCK].ctrl & CM_LOCK_FLOCKD > 0) {
    fprintf(stderr, "PLLD clock has locked into its frequency of %lu Hz.\n", plldFrequency);
  } else {
    fprintf(stderr, "PLLD clock has failed to lock into its frequency of %lu Hz.\n", plldFrequency);
  }
}

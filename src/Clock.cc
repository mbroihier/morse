
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
  uint32_t clockControlCopy = clkReg[EMMCCLK].ctrl;
  // kill the clock if busy
  if (clkReg[EMMCCLK].ctrl & CLK_CTL_BUSY) {
    do {
      fprintf(stderr, "EMMCCLK is busy\n");
      // turn off enable for graceful stop
      clkReg[EMMCCLK].ctrl = BCM_PASSWD || (clockControlCopy & (!CLK_CTL_ENAB));
    } while (clkReg[EMMCCLK].ctrl & CLK_CTL_BUSY);
    fprintf(stderr, "EMMCCLK has stopped\n");
  }
  clockControlCopy = clkReg[EMMCCLK].ctrl;

  // Set clock source to plld
  clkReg[EMMCCLK].ctrl = BCM_PASSWD | (CLK_CTL_SRC(CLK_CTL_SRC_PLLD) | (clockControlCopy & ~0xf));
  usleep(100);

  // Enable the EMMC clock
  clkReg[EMMCCLK].ctrl |= BCM_PASSWD | CLK_CTL_ENAB;

  // set GP0 Clock to PLLC
  clockControlCopy = clkReg[GP0CLK].ctrl;
  if (clkReg[GP0CLK].ctrl & CLK_CTL_BUSY) {
    fprintf(stderr, "GP0CLK is busy\n");
    do {
      // turn off enable and send kill - turning off enable doesn't seem to be enough
      fprintf(stderr, "Sending GP0CLK control %8.8x\n",
              BCM_PASSWD | (clockControlCopy & (!CLK_CTL_ENAB)) | CLK_CTL_KILL);
      clkReg[GP0CLK].ctrl = BCM_PASSWD | (clockControlCopy & (!CLK_CTL_ENAB)) | CLK_CTL_KILL;
    } while (clkReg[GP0CLK].ctrl & CLK_CTL_BUSY);
    fprintf(stderr, "GP0CLK has stopped\n");
  }
  clockControlCopy = clkReg[GP0CLK].ctrl;
  fprintf(stderr, "Current clock control copy: %8.8x\n", clockControlCopy);
  // must turn off kill

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
    if ((uint64_t)centerFrequency * divider < 200e6) {
      fprintf(stderr, "divider shouldn't get this small - %d\n", divider);
      continue;
    }
    if ((uint64_t)centerFrequency * divider > 1500e6) {
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
  usleep(100);
  double multiplier = (static_cast<double>(centerFrequency) * divider) / static_cast<double>(XOSC_FREQUENCY);
  uint32_t scaledMultiplier = multiplier * static_cast<double>(1 << 20);
  uint32_t integerPortion = scaledMultiplier >> 20;
  uint32_t fractionalPortion = scaledMultiplier & 0xfffff;
  clkReg[PLLC_FRAC].ctrl = BCM_PASSWD | fractionalPortion;
  usleep(100);
  fprintf(stderr, "Sending PLLC control command of %8.8x\n", BCM_PASSWD | integerPortion | (0x21 << 12));
  clkReg[PLLC_CTRL].ctrl = BCM_PASSWD | integerPortion | (0x21 << 12);  // PDIV of 1, PRSTN (start?)
  usleep(100);
  // must turn off kill while enabling GP0 clock
  clkReg[GP0CLK].ctrl = (clockControlCopy & ~0x3f) | BCM_PASSWD | CLK_CTL_SRC(CLK_CTL_SRC_PLLC) | CLK_CTL_ENAB;
  usleep(100);
  // check for frequency lock of PLLC
  fprintf(stderr, "CM_LOCK address %p\n", &clkReg[CM_LOCK].div);
  fprintf(stderr, "CM_LOCK value: %8.8x\n", clkReg[CM_LOCK].div);
  if (clkReg[CM_LOCK].div & CM_LOCK_FLOCKC > 0) {
    fprintf(stderr, "PLLC clock has locked into its frequency of %lu Hz.\n", pllcFrequency);
  } else {
    fprintf(stderr, "PLLC clock has failed to lock into its frequency of %lu Hz.\n", pllcFrequency);
  }

  pllCtl = clkReg[PLLC_CTRL].ctrl;
  pllFrac = clkReg[PLLC_FRAC].ctrl;
  pllPer = clkReg[PLLC_PER].ctrl;
  frequency = ((XOSC_FREQUENCY * ((uint64_t)pllCtl & 0x3ff) + (XOSC_FREQUENCY * (uint64_t)pllFrac) / (1 << 20)) /
               (2 * pllPer >> 1)) / ((pllCtl >> 12) & 0x7) * 2;
  fprintf(stderr, "PLL C frequency should now be %lu\n", frequency);
  fprintf(stderr, "Multiplier should be %f\n", static_cast<double>(scaledMultiplier)/static_cast<double>(1<<20));
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
  if (clkReg[CM_LOCK].div & CM_LOCK_FLOCKD > 0) {
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
  fprintf(stderr,
          "Clock initialization complete, all clocks (GP0, PLLC, PLLD, PCM) should be configured and running\n");
}

Clock::~Clock() {
  // before shutdown - look at lock
  fprintf(stderr, "Clock shutting down\n");
  if (clkReg[CM_LOCK].div & CM_LOCK_FLOCKC > 0) {
    fprintf(stderr, "PLLC clock has locked into its frequency of %lu Hz.\n", pllcFrequency);
  } else {
    fprintf(stderr, "PLLC clock has failed to lock into its frequency of %lu Hz.\n", pllcFrequency);
  }
  if (clkReg[CM_LOCK].div & CM_LOCK_FLOCKD > 0) {
    fprintf(stderr, "PLLD clock has locked into its frequency of %lu Hz.\n", plldFrequency);
  } else {
    fprintf(stderr, "PLLD clock has failed to lock into its frequency of %lu Hz.\n", plldFrequency);
  }
}


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

Class for creating Raspberry Pi Broadcom Clock HW Control

Mark Broihier 2021
*/

#ifndef INCLUDE_CLOCK_H_
#define INCLUDE_CLOCK_H_

#include <stdint.h>
#include <unistd.h>
#include "../include/GPIO.h"
#include "../include/Peripheral.h"

#define CM_BASE 0x00101000
#define CM_LEN 0x1660
#define CLK_CTL_BUSY (1 << 7)
#define CLK_CTL_KILL (1 << 5)
#define CLK_CTL_ENAB (1 << 4)
#define CLK_CTL_SRC(x) ((x) << 0)

#define CLK_CTL_SRC_PLLA 4
#define CLK_CTL_SRC_PLLC 5
#define CLK_CTL_SRC_PLLD 6

#define CLK_DIVI 5
#define CLK_DIV_DIVI(x) ((x) << 12)

#define BCM_PASSWD (0x5A << 24)

#define CORECLK (0x00000008 / 8)
#define PCMCLK  (0x00000098 /8)
#define GP0CLK  (0x00000070 / 8)
#define CM_PLLC (0x00000108 / 8)
#define EMMCCLK (0x000001d0 / 8)

#define PLLC_CTRL (0x00001120 / 8)
#define PLLD_CTRL (0x00001140 / 8)
#define PLLC_FRAC (0x00001220 / 8)
#define PLLD_FRAC (0x00001240 / 8)
#define PLLC_PER  (0x00001520 / 8)
#define PLLD_PER  (0x00001540 / 8)
#define PLLC_CORE (0x00001620 / 8)
#define PLLD_CORE (0x00001640 / 8)

#define CM_LOCK (0x00000114 /8)
#define CM_LOCK_FLOCKC (1 << 10)
#define CM_LOCK_FLOCKD (1 << 11)


#define XOSC_FREQUENCY 19200000LL

class Clock {
 private:
  typedef struct CLKCtrlReg {
    // See https://elinux.org/BCM2835_registers#CM
    uint32_t ctrl;
    uint32_t div;
  } CLKCtrlReg;

  uint64_t frequency;      // work variable
  uint32_t centerFrequency;
  uint64_t pllcFrequency;  // frequency of PLLC
  uint64_t plldFrequency;  // frequency of PLLD

  GPIO * gpio;

 public:
  volatile CLKCtrlReg *clkReg;
  void initClock();
  inline uint64_t getPLLCFrequency(){return pllcFrequency;}
  inline uint64_t getPLLDFrequency(){return plldFrequency;}
  explicit Clock(uint32_t centerFrequency, GPIO * gpio, Peripheral * peripheralUtil);
  ~Clock(void);
};
#endif  // INCLUDE_CLOCK_H_

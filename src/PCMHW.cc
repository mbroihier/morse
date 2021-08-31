
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

Raspberry Pi PCM Broadcom HW Control

Mark Broihier 2021
*/

#include "../include/PCMHW.h"

void PCMHW::initPCM() {
  pcmReg->ctrl = PCM_CTL_EN;
  fprintf(stderr, "PCMHW setup complete\n");
}

// change this to always produce a 1KHz clock which is 1 msec per clock
// for 10 words per second rate (500 symbols per min), that is 120 clocks per symbol
// therefore, for 5 words per second rate (250 symbols per min), that is 240 clocks per symbol
uint32_t PCMHW::setPCMFrequency(uint32_t rate) {
  uint32_t prediv = 9;  // don't know why 10 should be minimum prediv
  uint32_t frequency = 1000;  // use a 1 KHz (1 msec) timer to clock subsymbols
  double pcmFrequencyCtl = 0.0;
  fprintf(stderr, "symbol rate times upsample = %d\n", frequency);
  do {
    prediv++;
    pcmFrequencyCtl = clock->getPLLDFrequency() / static_cast<double>(frequency * prediv);
  } while (prediv < 1000 && ((pcmFrequencyCtl <= 2.0) || (pcmFrequencyCtl >= 4096.0)));
  fprintf(stderr, "pcmFrequencyCtl = %f\n", pcmFrequencyCtl);

  if (prediv > 1000 || pcmFrequencyCtl <= 2.0 || pcmFrequencyCtl >= 4096.0) {
    fprintf(stderr, "PCM prediv can be no more than 1000 for PCM Frequency Control out of range.\n"
            "prediv: %d pcpFrequencyCtl: %f\n", prediv, pcmFrequencyCtl);
    exit(-1);
  }
  fprintf(stderr, "PCM prediv is: %d\n", prediv);

  uint32_t pcmDivider = (uint32_t) pcmFrequencyCtl;
  uint32_t pcmDividerFraction = (uint32_t) (4096 * (pcmFrequencyCtl - static_cast<double>(pcmDivider)));
  fprintf(stderr, "Setting PCM clock controls to %d and %d\n", pcmDivider, pcmDividerFraction);
  // kill the clock if busy
  if (clock->clkReg[PCMCLK].ctrl & CLK_CTL_BUSY) {
    do {
      clock->clkReg[PCMCLK].ctrl = BCM_PASSWD | CLK_CTL_KILL;
    } while (clock->clkReg[PCMCLK].ctrl & CLK_CTL_BUSY);
  }
  fprintf(stderr, "PCM clock stopped, changing source to PLLD\n");
  // set PCM dividers and fractions
  clock->clkReg[PCMCLK].div = BCM_PASSWD | CLK_DIV_DIVI(pcmDivider) | pcmDividerFraction;
  // reenable the clock
  clock->clkReg[PCMCLK].ctrl = BCM_PASSWD | CLK_CTL_SRC(CLK_CTL_SRC_PLLD) | CLK_CTL_ENAB;

  pcmReg->transmitter = 1 << 30;  // 1 channel, 8 bits
  usleep(100);
  pcmReg->mode = (prediv - 1) << 10;  // prediv clock by prediv - 1 as per HW documentation
  usleep(100);
  pcmReg->ctrl |= 1 << 4 | 1 << 3;  // clear fifos
  usleep(100);
  pcmReg->dmaReq = 0x40 << 24 | 0x40 << 8;  // DMA Request when one slot is free
  usleep(100);
  pcmReg->ctrl |= 1 << 9;  // enable DMA
  usleep(100);
  pcmReg->ctrl |= 1 << 2;  // Start transmit of PCM

  // rate is words per min which turns out to be standardized to 0.120 seconds / subsymbol for 10 words per min
  // To get calculate the clocks per subsymbol for a rate we do this:
  // 120 clocks/subsymbol * 10 words/min / rate words/min = clocks per subsymbol
  return ( 1200 / rate );
}

  PCMHW::PCMHW(Clock * clock, Peripheral * peripheralUtil) {
  pcmReg = reinterpret_cast<PCMCtrlReg *>(peripheralUtil->mapPeripheralToUserSpace(PCM_BASE, PCM_LEN));
  this->clock = clock;
  initPCM();
}

PCMHW::~PCMHW() {
  fprintf(stderr, "Shutting down PCMHW\n");
}

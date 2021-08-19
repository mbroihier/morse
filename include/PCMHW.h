
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

Class for creating Raspberry Pi Broadcom PCM HW Control

Mark Broihier 2021
*/

#ifndef INCLUDE_PCMHW_H_
#define INCLUDE_PCMHW_H_
#include <stdint.h>
#include <unistd.h>
#include "../include/Clock.h"
#include "../include/Peripheral.h"

/* PWM mapping information */
#define PCM_BASE 0x00203000
#define PCM_FIFO 0x00000004
#define PCM_FIFO_SIZE 0x40
#define PCM_LEN 0x24
#define PCM_TX 2

/* PCM control bits */
#define PCM_CTL_EN   (1 << 0)

#define PCM_ENABLE_CHANNEL_1 (1 << 30)


class PCMHW {
 private:
  typedef struct PCMCtrlReg {
    uint32_t ctrl;         // 0x0, Control
    uint32_t fifo;         // 0x4, FIFO
    uint32_t mode;         // 0x8, Mode
    uint32_t receiver;     // 0xC, Receiver
    uint32_t transmitter;  // 0x10, Transmitter
    uint32_t dmaReq;       // 0x14, DREQ
    uint32_t intensity;    // 0x18, Intensity
    uint32_t intstc;       // 0x1C, ?
    uint32_t gray;         // 0x20, Gray
  } PCMCtrlReg;

  Clock * clock;
  volatile PCMCtrlReg * pcmReg;

 public:
  void initPCM();
  uint32_t setPCMFrequency(uint32_t rate);
  explicit PCMHW(Clock * clock, Peripheral * peripheralUtil);
  ~PCMHW(void);
};
#endif  // INCLUDE_PCMHW_H_


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

Class for making a DMA channel

Mark Broihier 2021
*/
#include "../include/DMAChannel.h"

DMAChannel::DMAMemHandle * DMAChannel::dmaMalloc(size_t size) {
  if (mailboxFD < 0) {
    mailboxFD = mbox_open();
    assert(mailboxFD >= 0);
  }

  // Make `size` a multiple of PAGE_SIZE
  size = ((size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

  DMAMemHandle *mem = reinterpret_cast<DMAMemHandle *>(malloc(sizeof(DMAMemHandle)));
  // Documentation: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
  mem->mbHandle = mem_alloc(mailboxFD, size, PAGE_SIZE, MEM_FLAG_L1_NONALLOCATING);
  mem->busAddr = mem_lock(mailboxFD, mem->mbHandle);
  mem->virtualAddr = mapmem(BUS_TO_PHYS(mem->busAddr), size);
  mem->size = size;

  assert(mem->busAddr != 0);

  fprintf(stderr, "MBox alloc: %d bytes, bus: %08X, virt: %08X\n", mem->size, mem->busAddr,
          (uint32_t)mem->virtualAddr);

  return mem;
}

void DMAChannel::dmaFree(DMAMemHandle *mem) {
  if (mem->virtualAddr == NULL) return;

  unmapmem(mem->virtualAddr, PAGE_SIZE);
  mem_unlock(mailboxFD, mem->mbHandle);
  mem_free(mailboxFD, mem->mbHandle);
  mem->virtualAddr = NULL;
}

void DMAChannel::dmaAllocBuffers(size_t subSymbolsSize, uint32_t clocksPerSubSymbol, GPIO * gpio) {
  dmaCBs = dmaMalloc((PCM_FIFO_SIZE + 2 * subSymbolsSize * clocksPerSubSymbol + 1) * sizeof(DMAControlBlock));
  commandPinToClock = dmaMalloc(sizeof(uint32_t));
  // note, this only works for the first 10 BCM GPIO pins
  *reinterpret_cast<uint32_t *>(commandPinToClock->virtualAddr) = (gpio->pinModeSettings & ~(7 << (gpio->pin * 3))) |
    (4 << (gpio->pin * 3));
  commandPinToInput = dmaMalloc(sizeof(uint32_t));
  *reinterpret_cast<uint32_t *>(commandPinToInput->virtualAddr) = gpio->pinModeSettings & ~(7 << (gpio->pin * 3));
}
void DMAChannel::dmaInitCBs(char * subSymbols, size_t subSymbolsSize, uint32_t clocksPerSubSymbol) {
  DMAControlBlock *cb;
  int index = 0;
  cb = ithCBVirtAddr(index);  // point to first control block - this is always used to fill the FIFO before
                              // transmission
  cb->txInfo = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP | DMA_DEST_DREQ | DMA_PERIPHERAL_MAPPING(PCM_TX);  // 2
  cb->src = commandPinToInputBusAddr();  // set the pin to input (won't send clock to the pin)
  cb->dest = PERI_BUS_BASE + PCM_BASE + PCM_FIFO;
  cb->txLen = 4 * (PCM_FIFO_SIZE + 1);
  cb->stride = 0;
  index++;
  cb->nextCB = ithCBBusAddr(index);
  int subSymbolIndex = 0;
  int clocks = 0;
  while (subSymbolIndex < subSymbolsSize) {
    fprintf(stderr, "control block index: %d subSymbolIndex: %d clocks %d\n",
            index, subSymbolIndex, clocks);
    // signal control block - clock or no clock
    cb = ithCBVirtAddr(index);
    cb->txInfo = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
    cb->src = subSymbols[subSymbolIndex] ? commandPinToClockBusAddr() : commandPinToInputBusAddr();
    cb->dest = PERI_BUS_BASE + GPIO_BASE + GPIO_FSEL;
    cb->txLen = 4;
    cb->stride = 0;
    index++;
    cb->nextCB = ithCBBusAddr(index);
    // delay control block - one subsymbol clock
    cb = ithCBVirtAddr(index);
    cb->txInfo = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP | DMA_DEST_DREQ | DMA_PERIPHERAL_MAPPING(PCM_TX);
    cb->src = ithCBBusAddr(0);  // Dummy data
    cb->dest = PERI_BUS_BASE + PCM_BASE + PCM_FIFO;
    cb->txLen = 4;
    cb->stride = 0;
    index++;
    cb->nextCB = ithCBBusAddr(index);
    clocks++;
    if (clocks == clocksPerSubSymbol) {
      subSymbolIndex++;  // go to next sub symbol if we've sent this one out enough times
      clocks = 0;
    }
  }
  // stop output of clock and DMA
  cb = ithCBVirtAddr(index);
  cb->txInfo = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
  cb->src = commandPinToInputBusAddr();
  cb->dest = PERI_BUS_BASE + GPIO_BASE + GPIO_FSEL;
  cb->txLen = 4;
  cb->stride = 0;
  cb->nextCB = 0;  // no more DMA commands

  // print out control blocks
  index = 0;
  cb = ithCBVirtAddr(index);
  bool notDone = true;
  do {
    fprintf(stderr, "%p TXINFO %8.8x index %5d\n", &cb->txInfo, cb->txInfo, index);
    fprintf(stderr, "%p SRC    %8.8x\n", &cb->src, cb->src);
    fprintf(stderr, "%p DEST   %8.8x\n", &cb->dest, cb->dest);
    fprintf(stderr, "%p LEN    %8.8x\n", &cb->txLen, cb->txLen);
    fprintf(stderr, "%p NEXT   %8.8x\n", &cb->nextCB, cb->nextCB);
    if (cb->nextCB == 0) {
      notDone = false;
    } else {
      index++;
      cb = ithCBVirtAddr(index);
    }
  } while (notDone);
}


void DMAChannel::dmaStart() {
  // Reset the DMA channel
  fprintf(stderr, "Starting DMA channel controller\n");
  dmaReg->cs = DMA_CHANNEL_ABORT;
  dmaReg->cs = 0;
  dmaReg->cs = DMA_CHANNEL_RESET;
  dmaReg->cbAddr = 0;

  dmaReg->cs = DMA_INTERRUPT_STATUS | DMA_END_FLAG;

  // Make cbAddr point to the first DMA control block and enable DMA transfer
  dmaReg->cbAddr = ithCBBusAddr(0);
  dmaReg->cs = DMA_PRIORITY(8) | DMA_PANIC_PRIORITY(8) | DMA_DISDEBUG;
  dmaReg->cs |= DMA_WAIT_ON_WRITES | DMA_ACTIVE;
}

bool DMAChannel::dmaIsRunning() {
  fprintf(stderr, "dmaReg->cs : %8.8x\n", dmaReg->cs);
  return ((dmaReg->cs & DMA_ACTIVE) != 0);
}

void DMAChannel::dmaEnd() {
  fprintf(stderr, "Stopping DMA channel controller\n");
  // Shutdown DMA channel.
  dmaReg->cs |= DMA_CHANNEL_ABORT;
  usleep(100);
  dmaReg->cs &= ~DMA_ACTIVE;
  dmaReg->cs |= DMA_CHANNEL_RESET;
  usleep(100);

  // Release the memory used by DMA
  dmaFree(dmaCBs);
  dmaFree(commandPinToClock);
  dmaFree(commandPinToInput);

  free(dmaCBs);
  free(commandPinToClock);
  free(commandPinToInput);
}

DMAChannel::DMAChannel(char * subSymbols, size_t subSymbolsSize, uint32_t clocksPerSubSymbol, uint32_t channel,
                       GPIO * gpio, Peripheral * peripheralUtil) {
  dmaAllocBuffers(subSymbolsSize, clocksPerSubSymbol, gpio);
  uint8_t * dmaBasePtr = reinterpret_cast<uint8_t *>(peripheralUtil->mapPeripheralToUserSpace(DMA_BASE, PAGE_SIZE));
  dmaReg = reinterpret_cast<DMACtrlReg *>(dmaBasePtr + channel * 0x100);
  this->channel = channel;
  fprintf(stderr, "Constructing object for DMA channel %d\n", channel);
  dmaInitCBs(subSymbols, subSymbolsSize, clocksPerSubSymbol);
}
DMAChannel::~DMAChannel(void) {
  dmaEnd();
}

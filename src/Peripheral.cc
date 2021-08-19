
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

Utilities to handle mapping of peripherals to user space memory

Mark Broihier 2021
*/

#include "../include/Peripheral.h"

void * Peripheral::mapPeripheralToUserSpace(uint32_t addr, size_t size) {
  int mem_fd;
  // Check mem(4) about /dev/mem
  if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
    perror("Failed to open /dev/mem: ");
    exit(-1);
  }

  uint32_t *result = reinterpret_cast<uint32_t *>(mmap(
                                                       NULL,
                                                       size,
                                                       PROT_READ | PROT_WRITE,
                                                       MAP_SHARED,
                                                       mem_fd,
                                                       PERI_PHYS_BASE + addr));

  close(mem_fd);

  fprintf(stderr, "mmap to address %8.8x\n", addr);
  if (result == MAP_FAILED) {
    perror("mmap error: ");
    exit(-1);
  }

  if (!listOfMappings) {
    listOfMappings = reinterpret_cast<Mappings *>(malloc(sizeof(Mappings)));
    listOfMappingsHead = listOfMappings;
  } else {
    listOfMappings->nextMap = reinterpret_cast<Mappings *>(malloc(sizeof(Mappings)));
    listOfMappings = listOfMappings->nextMap;
  }
  listOfMappings->offsetFromBase = addr;
  listOfMappings->peripheralAddress = result;
  listOfMappings->size = size;
  listOfMappings->nextMap = 0;

  return result;
}

void Peripheral::unmapPeripherals() {
  Mappings * toFree = 0;
  while (listOfMappingsHead) {
    fprintf(stderr, "Freeing memory related to peripheral offset: %8.8x\n", listOfMappingsHead->offsetFromBase);
    munmap(listOfMappingsHead->peripheralAddress, listOfMappingsHead->size);
    toFree = listOfMappingsHead;
    listOfMappingsHead = listOfMappingsHead->nextMap;
    free(toFree);
  }
  listOfMappingsHead = 0;
}

Peripheral::Peripheral() {
  PERI_PHYS_BASE = bcm_host_get_peripheral_address();
  listOfMappings = 0;
}

Peripheral::~Peripheral() {
  fprintf(stderr, "Shutting down Peripheral\n");
  if (listOfMappingsHead) {
    unmapPeripherals();
  }
}

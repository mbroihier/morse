
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

Mark Broihier
*/

#include <ctype.h>
#include <signal.h>

#include "../include/Clock.h"
#include "../include/DMAChannel.h"
#include "../include/GPIO.h"
#include "../include/Peripheral.h"
#include "../include/PCMHW.h"
#include "../include/mailbox.h"

// morse code translation table taken from morse.cpp in https://github.com/F5OEO/rpitx
#define MORSECODES 37

typedef struct morse_code {
  uint8_t ch;
  const char ditDah[8];
} Morsecode;

const Morsecode translationTable[]  = {
                                       {' ', "    "},
                                       {'0', "-----  "},
                                       {'1', ".----  "},
                                       {'2', "..---  "},
                                       {'3', "...--  "},
                                       {'4', "....-  "},
                                       {'5', ".....  "},
                                       {'6', "-....  "},
                                       {'7', "--...  "},
                                       {'8', "---..  "},
                                       {'9', "----.  "},
                                       {'A', ".-  "},
                                       {'B', "-...  "},
                                       {'C', "-.-.  "},
                                       {'D', "-..  "},
                                       {'E', ".  "},
                                       {'F', "..-.  "},
                                       {'G', "--.  "},
                                       {'H', "....  "},
                                       {'I', "..  "},
                                       {'J', ".---  "},
                                       {'K', "-.-  "},
                                       {'L', ".-..  "},
                                       {'M', "--  "},
                                       {'N', "-.  "},
                                       {'O', "---  "},
                                       {'P', ".--.  "},
                                       {'Q', "--.-  "},
                                       {'R', ".-.  "},
                                       {'S', "...  "},
                                       {'T', "-  "},
                                       {'U', "..-  "},
                                       {'V', "...-  "},
                                       {'W', ".--  "},
                                       {'X', "-..-  "},
                                       {'Y', "-.--  "},
                                       {'Z', "--..  "}
};

size_t messageToMorse(const char * message, char * encodedMessage, size_t maxEncodedLength) {
  size_t messageLength = strlen(message);
  uint32_t encodedMessageIndex = 0;
  bool found;
  for (uint32_t index = 0; index < messageLength; index++) {
    char workingCharacter = toupper(message[index]);
    found = false;
    for (uint32_t tableIndex = 0; tableIndex < MORSECODES; tableIndex++) {
      if (workingCharacter == translationTable[tableIndex].ch) {
        const char * characterPattern = translationTable[tableIndex].ditDah;
        uint32_t patternSize = strlen(characterPattern);
        found = true;
        if (encodedMessageIndex + 4*patternSize < maxEncodedLength) {
          for (uint32_t patternIndex = 0; patternIndex < patternSize; patternIndex++) {
            if (characterPattern[patternIndex] == '.') {
              encodedMessage[encodedMessageIndex++] = 1;
              encodedMessage[encodedMessageIndex++] = 0;
            } else if (characterPattern[patternIndex] == '-') {
              encodedMessage[encodedMessageIndex++] = 1;
              encodedMessage[encodedMessageIndex++] = 1;
              encodedMessage[encodedMessageIndex++] = 1;
              encodedMessage[encodedMessageIndex++] = 0;
            } else {
              encodedMessage[encodedMessageIndex++] = 0;
            }
          }
        } else {
          fprintf(stderr, "Error during encoding - not enough space in encoded message buffer\n");
          exit(-1);
        }
      }
    }
    if (!found) {
      fprintf(stderr, "Error during encoding - character not found in translation table\n");
      exit(-1);
    }
  }
  fprintf(stdout, "Encoded message:\n");
  for (uint32_t index = 0; index < encodedMessageIndex; index++) {
    fprintf(stdout, "%1.1d", encodedMessage[index]);
  }
  fprintf(stdout, "\n");
  return(encodedMessageIndex);
}

bool exitLoop = false;

void sigint_handler(int signo) {
  if (signo == SIGINT) {
    fprintf(stdout, "\nUser termination request\n");
    exitLoop = true;
  }
}

int main(int argc, char ** argv) {
  uint32_t frequency = 0;
  uint32_t symbolRate = 0;
  const char * message = 0;

  signal(SIGINT, sigint_handler);

  if (argc != 4) {
    fprintf(stdout, "Usage: sudo ./morse <frequency> <transmission rate> <message - in quotes>\n");
    exit(-1);
  }
  frequency = atoi(argv[1]);
  symbolRate = atoi(argv[2]);
  message = argv[3];

  Peripheral peripheralUtil;  //  create an object to reference peripherals
  GPIO gpio(4, &peripheralUtil);  // Use pin GPIO 4 (BCM)
  Clock clock(frequency, &gpio, &peripheralUtil);
  PCMHW pcm(&clock, &peripheralUtil);
  uint32_t clocksPerSubSymbol = pcm.setPCMFrequency(symbolRate);

  size_t messageLen = strlen(message) * 7 * 4;  // 7 dit dahs, 4 bytes per dit dah (maximum)
  char * transmissionBuffer = reinterpret_cast<char *>(malloc(messageLen));
  messageLen = messageToMorse(message, transmissionBuffer, messageLen);
  DMAChannel dma(transmissionBuffer, messageLen, clocksPerSubSymbol, 5, &gpio, &peripheralUtil);
  dma.dmaStart();
  fprintf(stdout, "Message transmission started.\n");
  int forceTermination = 0;
  while (dma.dmaIsRunning() && !exitLoop) {
    fprintf(stdout, "DMA Channel is still running/message still being sent, poll cycle %d\n", forceTermination);
    sleep(1.0);
    if (forceTermination++ > 120) break;
  }
  free(transmissionBuffer);
  return 0;
}

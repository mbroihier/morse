#
#
cmake_minimum_required(VERSION 3.7)
project(morse)
set(PROJECT_VERSION "0.01")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_BINARY_DIR}/include")
include_directories("/opt/vc/include")
link_directories("/opt/vc/lib")

#set(MORSE_SRC src/morse.cc src/Clock.cc src/DMAChannel.cc src/GPIO.cc  src/mailbox.cc src/Peripheral.cc )
set(MORSE_SRC src/morse.cc src/Clock.cc src/GPIO.cc src/PCMHW.cc src/Peripheral.cc src/mailbox.cc src/DMAChannel.cc)
add_executable(morse ${MORSE_SRC})
target_link_libraries(morse bcm_host)

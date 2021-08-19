# Morse

This project implements a Morse code transmitter.  It uses ideas from rpitx (https://github.com/F5OEO/rpitx) and from my servo daemon project (https://github.com/mbroihier/servod).

This version has only been tested on RPI 3's.  It should work on a PI 0 without modification.  It will not work on a PI 4 without modification.

## To compile
```
$ git clone https://github.com/mbroihier/morse
$ cd morse
$ mkdir build
$ cd build
$ cmake ..
$ make
```

## To Use
In the build directory:
```
$ sudo ./morse 28100000 10 "cq cq kg5yje"
```
This sends the message "cq cq kg5yje" at 10 words per minute on frequency 28.1 MHz


## Notes

mailbox.cc is not my code and has a Copyright issued by Broadcom Europe Ltd.  Please read its prologue for proper use and distribution.

It is your responsibility to follow all FCC regulations.  The transmission of this signal will be performed from physical pin 7 of the 40 pin connector.  This signal is generated using a square wave clock and, therefore, requires an appropriate bandpass filter prior to connection to an antenna.
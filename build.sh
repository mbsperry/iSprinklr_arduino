#!/bin/sh
/home/pi/bin/arduino-cli compile --fqbn arduino:samd:arduino_zero_edbg serial.ino

/home/pi/bin/arduino-cli upload -p /dev/ttyAMA0 --fqbn arduino:samd:arduino_zero_edbg serial.ino

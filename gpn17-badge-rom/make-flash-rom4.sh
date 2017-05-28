#!/bin/bash -x

# make with arduino-builder and FLASH to rom4 of GPN badge

ROM=rom4
ADDR=0x202000

set -e

rm -rvf /tmp/arduino_build_111111 /tmp/arduino_cache_111111
mkdir /tmp/arduino_build_111111 /tmp/arduino_cache_111111

/opt/arduino-1.8.2/arduino-builder -dump-prefs -logger=machine -hardware /opt/arduino-1.8.2/hardware -hardware /home/tb/.arduino15/packages -tools /opt/arduino-1.8.2/tools-builder -tools /opt/arduino-1.8.2/hardware/tools/avr -tools /home/tb/.arduino15/packages -built-in-libraries /opt/arduino-1.8.2/libraries -libraries /home/tb/Arduino/libraries -fqbn=esp8266:esp8266:multirom:CpuFrequency=160,UploadSpeed=921600,FlashSize=$ROM -ide-version=10802 -build-path /tmp/arduino_build_111111 -warnings=none -build-cache /tmp/arduino_cache_111111 -prefs=build.warn_data_percentage=75 -prefs=runtime.tools.mkspiffs.path=/home/tb/.arduino15/packages/esp8266/tools/mkspiffs/0.1.2 -prefs=runtime.tools.esptool.path=/home/tb/.arduino15/packages/esp8266/tools/esptool/0.4.9 -prefs=runtime.tools.xtensa-lx106-elf-gcc.path=/home/tb/.arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2 -prefs=runtime.tools.esptool2.path=/home/tb/.arduino15/packages/esp8266/tools/esptool2/1.0.0 -verbose pong-controller.ino

/opt/arduino-1.8.2/arduino-builder -compile -logger=machine -hardware /opt/arduino-1.8.2/hardware -hardware /home/tb/.arduino15/packages -tools /opt/arduino-1.8.2/tools-builder -tools /opt/arduino-1.8.2/hardware/tools/avr -tools /home/tb/.arduino15/packages -built-in-libraries /opt/arduino-1.8.2/libraries -libraries /home/tb/Arduino/libraries -fqbn=esp8266:esp8266:multirom:CpuFrequency=160,UploadSpeed=921600,FlashSize=$ROM -ide-version=10802 -build-path /tmp/arduino_build_111111 -warnings=none -build-cache /tmp/arduino_cache_111111 -prefs=build.warn_data_percentage=75 -prefs=runtime.tools.mkspiffs.path=/home/tb/.arduino15/packages/esp8266/tools/mkspiffs/0.1.2 -prefs=runtime.tools.esptool.path=/home/tb/.arduino15/packages/esp8266/tools/esptool/0.4.9 -prefs=runtime.tools.xtensa-lx106-elf-gcc.path=/home/tb/.arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2 -prefs=runtime.tools.esptool2.path=/home/tb/.arduino15/packages/esp8266/tools/esptool2/1.0.0 -verbose pong-controller.ino

/home/tb/.arduino15/packages/esp8266/tools/esptool/0.4.9/esptool -vv -cd nodemcu -cb 921600 -cp /dev/ttyUSB0 -ca $ADDR -cf /tmp/arduino_build_111111/pong-controller.ino_rboot.bin

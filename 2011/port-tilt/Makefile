CC=avr-gcc
CFLAGS=-g -Os -Wall -mcall-prologues -mmcu=atmega328p
ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS)
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref	
OBJ2HEX=avr-objcopy 



SERIAL = /dev/tty.usbserial-m0000106
# $(ls /dev/*tty.usb*)

####### Files:

SOURCES	= monome-port.c twi.c
OBJECTS	= monome-port.o twi.o
TARGET	= monome-port

####### Implicit rules

.SUFFIXES: .c .o .cpp .cc .cxx .C

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

%.elf:	$(OBJECTS)


####### Build rules

$(TARGET):	$(OBJECTS)
		$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)
		$(CC) $(ALL_CFLAGS) $(OBJECTS) --output $(TARGET).elf $(LDFLAGS)	
		$(OBJ2HEX) -R .eeprom -O ihex $(TARGET) $(TARGET).hex

erase:
	avrdude -p m168 -b 115200 -P usb -c avrispmkII -e 

fuses: 
	avrdude -p m328p -b 115200 -P usb -c avrispmkII -U lfuse:w:0xff:m -U hfuse:w:0xdc:m -U efuse:w:0x03:m

flash: $(TARGET).hex
	avrdude -p m168 -b 115200 -P usb -c avrispmkII -e -V -D -U flash:w:$(TARGET).hex:i

p: $(TARGET).hex
	avrdude -p m328p -b 115200 -P usb -c avrispmkII -U lfuse:w:0xcf:m -U hfuse:w:0xdf:m -U efuse:w:0x01:m -U flash:w:$(TARGET).hex:i

boot: $(TARGET).hex
	avrdude -p m328p -b 115200 -P $(SERIAL) -c arduino -e -D -U flash:w:$(TARGET).hex:i


####### Compile

monome-port.o:		monome-port.c
twi.o:				twi.c
CC=avr-gcc
CFLAGS=-g -Os -Wall -mcall-prologues -mmcu=atmega328p
ALL_CFLAGS = -I. $(CFLAGS)
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref	
OBJ2HEX=avr-objcopy 

SERIAL = /dev/tty.usbserial-m0000310

###### i2c params
ADDR := 102


####### Files:

SOURCES	= grid-op.c twi.c button.c
OBJECTS	= grid-op.o twi.o button.o
TARGET	= grid-op

####### Implicit rules

.SUFFIXES: .c .o .cpp .cc .cxx .C

.c.o:
	$(CC) -c -DADDR="$(ADDR)" $(CFLAGS) $(INCPATH) -o $@ $<

%.elf:	$(OBJECTS)


####### Build rules

$(TARGET):	$(OBJECTS)
		$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)
		$(CC) $(ALL_CFLAGS) $(OBJECTS) --output $(TARGET).elf $(LDFLAGS)	
		$(OBJ2HEX) -R .eeprom -O ihex $(TARGET) $(TARGET).hex

erase:
	avrdude -p m328p -b 115200 -P usb -c avrispmkII -e 

fuses: 
	avrdude -p m328p -b 115200 -P usb -c avrispmkII -U lfuse:w:0xcf:m -U hfuse:w:0xdf:m -U efuse:w:0x01:m

flash: $(TARGET).hex
	avrdude -p m328p -b 115200 -P usb -c avrispmkII -e -V -D -U flash:w:$(TARGET).hex:i

p: $(TARGET).hex
	avrdude -p m328p -b 115200 -P usb -c avrispmkII -U lfuse:w:0xff:m -U hfuse:w:0xdf:m -U efuse:w:0x03:m -U flash:w:$(TARGET).hex:i

boot: $(TARGET).hex
	avrdude -p m328p -b 115200 -P $(SERIAL) -c arduino -e -D -U flash:w:$(TARGET).hex:i


####### Compile

grid-op.o:		grid-op.c
twi.o:			twi.c
button.o:		button.c
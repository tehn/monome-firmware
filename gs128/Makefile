CC=avr-gcc
CFLAGS=-g -Os -Wall -mcall-prologues -mmcu=atmega325
ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS)
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref	
OBJ2HEX=avr-objcopy 


####### Files:

SOURCES       = 128.c \
				button.c

OBJECTS	      = 128.o \
				button.o

TARGET=	128


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
	avrdude -p m325 -b 115200 -P usb -c avrispmkII -e 

fuses: 
	avrdude -p m325 -b 115200 -P usb -c avrispmkII -U lfuse:w:0xff:m

program: $(TARGET).hex
	avrdude -p m325 -b 115200 -P usb -c avrispmkII -e -D -U flash:w:$(TARGET).hex:i -U lfuse:w:0xff:m

pro: $(TARGET).hex
	avrdude -p m325 -b 115200 -P usb -c avrispmkII -e -D -U flash:w:$(TARGET).hex:i

####### Compile

128.o:		128.c
button.o:	button.c

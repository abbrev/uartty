F_CPU = 16000000L
CC = avr-gcc
LD = avr-gcc
CFLAGS += -mmcu=atmega328p -D F_CPU=$(F_CPU) -O2 --std=gnu99
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump

SRC := uartty.c
ASRC :=

OBJ = $(SRC:.c=.o) $(ASRC:.S=.o)

CFLAGS += -Wall #-O2 #-g

all: demo.hex
avr: all

uartty.o: uartty.h uartty-config.h

demo.elf: demo.o uartty.o

%.elf: $(OBJ)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@


.PHONY: obj avr
obj: $(OBJ)

%.o: %.c
	$(CC) -c $(CFLAGS) $^

%.elf: $(OBJ)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

%.lss: %.elf
	$(OBJDUMP) -h -S $< >$@

burn: demo.hex
	@read -p 'press enter when ready to burn: '
	avrdude -P /dev/ttyUSB0 -p m328p -c arduino -v -v -U flash:w:demo.hex


clean:
	rm -f demo *.elf $(OBJ) demo.hex *.lss

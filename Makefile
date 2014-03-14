ifeq ($(TARGET),avr)
F_CPU = 16000000L
CC = avr-gcc
LD = avr-gcc
CFLAGS += -mmcu=atmega328p -gstabs -D F_CPU=$(F_CPU) -O2
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
else
CFLAGS += -g
LDFLAGS += -lm -g
OBJCOPY = objcopy
OBJDUMP = objdump
CFLAGS += -DBIG_TARGET=1
endif

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

ifeq ($(TARGET),avr)
burn: demo.hex
	@read -p 'press enter when ready to burn: '
	avrdude -P /dev/ttyUSB0 -p m328p -c arduino -v -v -U flash:w:demo.hex
else
burn:
	@echo error: burn is for avr only!
	@false
endif


clean:
	rm -f demo *.elf $(OBJ) demo.hex *.lss

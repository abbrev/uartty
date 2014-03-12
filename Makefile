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

all:
avr: all

uartty.o: uartty.h

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

clean:
	rm -f synth *.elf $(OBJ) synth.hex *.lss wave.c

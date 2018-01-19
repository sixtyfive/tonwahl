# Makefile für das Projekt MFV-Wählscheibe
# Für avr-gcc; ich benutze WinAVR 2010.
# Erstellen mit "make", programmieren mit avrpp mit "make program".
# Schaltung zum Einbau in alte Telefone,
# ermöglicht Tonwahl trotz Fingerlochscheibe
# sowie programmierbare Kurzwahl (10 Nummern à 22 Ziffern)
# Diese Version (ursprünglich mfv3.c) kommt ohne Erdtaste aus;
# die Sonderfunktionen werden durch langes Halten der Wählscheibe am Fingeranschlag erreicht.
# Für AVR-Mikrocontroller (DEVICE) ATtiny25, ATtiny45 oder ATtiny85.
# Für Quarzfrequenzen (F_CPU) zwischen 10 und 20 MHz.

DEVICE  = attiny25
F_CPU   = 14318180
COMPILE = avr-gcc -Wall -Os -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)

all: $(F_CPU).elf size disasm

hex: $(F_CPU).hex
disasm: $(F_CPU).lst

$(F_CPU).elf: mfv.c
	$(COMPILE) -o $@ $<

%.hex: $(F_CPU).elf
	avr-objcopy -j .text -j .data -j .eeprom -j .fuse -j .signature -O ihex $< $@

size: $(F_CPU).elf
	@echo
	@avr-size -C --mcu=$(DEVICE) $<

%.lst: $(F_CPU).elf
	avr-objdump -d $< > $@

.PHONY: clean program fuse flash
clean:
	-rm -rf *.elf *.lst

program: $(F_CPU).elf
	avrpp -8 -ff $<

flash: $(F_CPU).elf
	avrpp -8 $<

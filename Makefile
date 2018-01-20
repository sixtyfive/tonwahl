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
F_CPU   = 14318180 # Hz

# Keine Warnungen, da der Quelltext unter GCC 7.2 ohnehin zwei Dutzend
# davon erzeugt...
#
# avr-gcc, sowie davor avr-binutils und danach avr-libc können für Linux
# nach Anleitung unter
# http://www.nongnu.org/avr-libc/user-manual/install_tools.html
# installiert werden (20.1.2018). Unter Windows muss WinAVR genutzt
# werden, welches allerdings anscheinend veraltete Versionen dieser
# drei Projekte nutzt.
COMPILE = avr-gcc -w -Os -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)

all: $(F_CPU).elf size disasm

hex: $(F_CPU).hex
disasm: $(F_CPU).lst

$(F_CPU).elf: mfv.c
	$(COMPILE) -o $@ $<

%.hex: $(F_CPU).elf
	avr-objcopy -j .text -j .data -j .eeprom -j .fuse -j .signature -O ihex $< $@

size: $(F_CPU).elf
	@echo
	# Befehl funktioniert *ausschliesslich* wenn binutils mit
	# https://raw.githubusercontent.com/embecosm/winavr/master/patches/binutils/2.19/30-binutils-2.19-avr-size.patch
	# gepatcht wurde. Der Patch konnte mit binutils-2.29 noch
	# angewandt werden (20.1.2018).
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

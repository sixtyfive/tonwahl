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

DEVICE  = attiny85
F_CPU   = 16000000

# Keine Warnungen, da der Quelltext unter GCC 7.2 ohnehin zwei Dutzend
# davon erzeugt...
#
# avr-gcc, sowie davor avr-binutils und danach avr-libc können für Linux
# nach Anleitung unter
# http://www.nongnu.org/avr-libc/user-manual/install_tools.html
# installiert werden (20.1.2018). Unter Windows muss WinAVR genutzt
# werden, welches allerdings anscheinend veraltete Versionen dieser
# drei Projekte nutzt.
AVR_GCC = avr-gcc -w -Os -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)
AVRDUDE = avrdude -v -p$(DEVICE)

all: $(F_CPU).elf size disasm hex

hex: $(F_CPU).hex
disasm: $(F_CPU).lst

$(F_CPU).elf: mfv.c
	$(AVR_GCC) -o $@ $<

# Projekt ist wie oben angegeben gedacht zum Programmieren mittels
# `avrpp` (*nicht* AVRpp sondern ein Teil von avrxtool32; siehe
# http://elm-chan.org/works/avrx/report_e.html#AVRXP). Zu dessen
# Verwendung wird der ebenfalls unter dieser URL beschriebene
# Parallelport-Programmer benötigt. Es gäbe zwar auch, passend zu
# einem weiterhin dort beschriebenen Seriellport-Adapter, auch
# `avrsp`, aber die wenigsten modernen Rechner besitzen noch den
# einen oder anderen Port. Da der zur Verwendung via USB zur Ver-
# fügung stehende `avrdude` nicht die Fähigkeit besitzt, in einem
# Zug die Fuses zu setzen und die Firmware zu schreiben, wurde
# unten das Objekt `.fuse` entfernt, so dass die .hex-Datei nur
# noch die verbleibenden gelisteten Objekte enthält.
%.hex: $(F_CPU).elf
	avr-objcopy -j .text -j .data -O ihex $< $@

# Befehl funktioniert *ausschliesslich* wenn binutils mit
# https://raw.githubusercontent.com/embecosm/winavr/master/patches/binutils/2.19/30-binutils-2.19-avr-size.patch
# gepatcht wurde. Der Patch konnte mit binutils-2.29 noch
# angewandt werden (20.1.2018).
size: $(F_CPU).elf
	@echo
	@avr-size -C --mcu=$(DEVICE) $<

%.lst: $(F_CPU).elf
	avr-objdump -d $< > $@

.PHONY: clean fuse flash
clean:
	-rm -rf *.elf *.lst *.hex

# Fuses für Arduino-Bootloader:
# - low:      0xE2
# - high:     0xD7
# - extended: 0xFF
# (int. osc., 8 MHz, startup time: 6 CK/14CK + 64ms - def. /
#  preserve EEPROM through erase, serial program downloading /
#  SUT0, CKSEL3, CKSEL2, CKSEL0, SPIEN, EESAVE)
fuse-reset:
	$(AVRDUDE) -U lfuse:w:0xE2:m -U hfuse:w:0xD7:m

# Fuses wie im ursprünglichen Makefile:
# - low:      0x5F
# - high:     0xD5
# - extended: 0xFF
# (ext. crystal, 8 MHz, startup time: 16K CK/14CK + 0ms /
#  divide clock by 8 internally, preserver EEPROM through erase, serial
#  program downloading /
#  CKDIV8, SUT1, SPIEN, EESAVE, BODLEVEL1)
fuse:
	$(AVRDUDE) -U lfuse:w:0x5F:m -U hfuse:w:0xD5:m

flash: $(F_CPU).hex
	$(AVRDUDE) -U flash:w:$(F_CPU).hex:i

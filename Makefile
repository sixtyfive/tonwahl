# Makefile für das Projekt MFV-Wählscheibe
# (Schaltung zum Einbau in alte Telefone,
# ermöglicht Tonwahl trotz Fingerlochscheibe
# sowie programmierbare Kurzwahl
# (10 Nummern à 30 Ziffern)
# Bei ATtiny25 sollte auf 15 oder 20 Ziffern gestutzt werden

PROJECT = mfv2
DEVICE  = attiny85
TOOLPREFIX = /opt/arduino/hardware/tools/avr
COMPILE = $(TOOLPREFIX)/bin/avr-gcc -Os -mmcu=$(DEVICE) \
					  -DF_CPU=16000000 # 16.00000 MHz
          # -DF_CPU=17280000 # 17.28000 MHz
          # -DF_CPU=14318180 # 14.31818 MHz
					# -Wall # Geht mir auf die Nerven
AVROBJCOPY = $(TOOLPREFIX)/bin/avr-objcopy
AVRSIZE = $(TOOLPREFIX)/bin/avr-size
AVROBJDUMP = $(TOOLPREFIX)/bin/avr-objdump
AVRDUDE = $(TOOLPREFIX)/bin/avrdude -C$(TOOLPREFIX)/etc/avrdude.conf -v -pattiny85 -cusbasp -Pusb

all: $(PROJECT).hex size disasm

disasm: $(PROJECT).lst

$(PROJECT).elf: mfv2.c
	$(COMPILE) -o $@ $<

%.hex: $(PROJECT).elf
	# Mit fuses und signatur
	# avr-objcopy -j .text -j .data -j .fuse -j .signature -O ihex $< $@
	#
	# Ohne fuses und signatur - fuses werden im avrdude-Aufruf mitgegeben
	$(AVROBJCOPY) -j .text -j .data -O ihex $< $@  

size: $(PROJECT).elf
	@echo
	@$(AVRSIZE) -C --mcu=$(DEVICE) $<

%.lst: $(PROJECT).elf
	$(AVROBJDUMP) -d $< > $@

.PHONY: clean program fuse flash
clean:
	-rm -rf $(PROJECT).elf $(PROJECT).lst $(PROJECT).hex

# Nicht installiert...
# program: $(PROJECT).hex
# 	avrpp -8 -ff $<

flash: $(PROJECT).hex
	# Nicht installiert...
	#	avrpp -8 $<
	#
	# Mit fuses! (s.o.)
	$(AVRDUDE) -U lfuse:w:0x5F:m -U hfuse:w:0xD5:m flash:w:$(PROJECT).hex:i

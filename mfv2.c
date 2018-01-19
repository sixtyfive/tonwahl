/* Programm für ATtiny25
 * „h#s“ Henrik Haftmann, TU Chemnitz, 31. März 2009 - 8. Oktober 2015
 * tabsize = 8, encoding = utf-8 (ohne Windows-Header BOM)
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/fuse.h>
#include <avr/signature.h>

FUSES={
 0x5F,	// Taktteiler /8, kein Taktausgang, Startup=16398CK, Quarzoszillator ≥ 8 MHz
 0xD5,	// RESET, EEPROM-Inhalt behalten, Brownout bei 2,7 V
 0xFF,	// Keine Selbstprogrammierung
};

// #define FILTER	// Rufnummern-Filter aktivieren

/************
 * Hardware *
 ************/
/*
Verwendung der Ports:
PB0	(5)	PCINT0	Wählscheiben-Kontakt "nsi", 1 = unterbrochen
PB1	(6)	OC1A	PWM-Tonausgang, reichlich (250 kHz) Trägerfrequenz
PB2	(7)	PCINT2	Erdtaste (wie Shift-Taste), 0 = gedrückt
PB3	(2)	XIN	Quarz
PB4	(3)	XOUT	Quarz
PB5	(1)	!RESET	frei

Es ist viel schwieriger, einen Adapter zum Zwischenschalten
(ohne Modifikation des Telefonapparates) zu bauen.
Der Wählscheiben-Kontakt "nsa" wird nicht benötigt.

Der EEPROM des AVR ermöglicht das Abspeichern von Nummern zur Kurzwahl.
Funktion:
 1. Ein-Finger-Bedienung
 Kurzwahl: Erde (kurz) - Ziffer
 Kurzwahl speichern: Erde (lang) - Rufnummer - Erde - Ziffer
 2. Zwei-Finger-Bedienung
 Wahlwiederholung: Erde + "1"
 Speichern der zuletzt gewählten Nummer: Erde + "2" - Ziffer
 Kurzwahl speichern (wie oben): Erde + "3" - Rufnummer - Erde - Ziffer
 Sonderzeichen senden:	Erde + "4..7" -> A..D
			Erde + "8" -> *
			Erde + "9" -> #
 Letzte Rufnummer löschen: Erde + "0"

Rufnummern-Filter:
 Alles was extra Geld kostet (Handy, Sonderrufnummern, Ausland; gültig für Deutschland):
 00	Ausland
 01	Dienste (0180), Handys, Call-By-Call
 070	Private Nummern (nie gesehen)
 090	Premium-Dienste (0900)
 118	Telefonauskunft
Der Filter ist nicht beim Speichern oder Wählen einer Kurzwahl wirksam!
Daher lassen sich bestimmte Nummern im Kurzwahlspeicher ablegen.
0800 ist erst mal nicht gesperrt; kann trotzdem (bei VoIP) Geld kosten - weiß nicht.
*/

// Signal-Zeiten
#define TON 0.14
#define PAUSE 0.06

typedef unsigned char BYTE;
typedef unsigned short WORD;

// Sinustabelle, Mittelwert und Amplitude = 73
PROGMEM const BYTE SinTab[256]={
 73, 75, 77, 78, 80, 82, 84, 85, 87, 89, 91, 92, 94, 96, 98, 99,
101,103,104,106,107,109,111,112,114,115,116,118,119,121,122,123,
125,126,127,128,129,131,132,133,134,135,136,137,137,138,139,140,
140,141,142,142,143,143,144,144,145,145,145,145,146,146,146,146,
146,146,146,146,146,145,145,145,145,144,144,143,143,142,142,141,
140,140,139,138,137,137,136,135,134,133,132,131,129,128,127,126,
125,123,122,121,119,118,116,115,114,112,111,109,107,106,104,103,
101, 99, 98, 96, 94, 92, 91, 89, 87, 85, 84, 82, 80, 78, 77, 75,
 73, 71, 69, 68, 66, 64, 62, 61, 59, 57, 55, 54, 52, 50, 48, 47,
 45, 43, 42, 40, 39, 37, 35, 34, 32, 31, 30, 28, 27, 25, 24, 23,
 21, 20, 19, 18, 17, 15, 14, 13, 12, 11, 10,  9,  9,  8,  7,  6,
  6,  5,  4,  4,  3,  3,  2,  2,  1,  1,  1,  1,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  3,  3,  4,  4,  5,
  6,  6,  7,  8,  9,  9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20,
 21, 23, 24, 25, 27, 28, 30, 31, 32, 34, 35, 37, 39, 40, 42, 43,
 45, 47, 48, 50, 52, 54, 55, 57, 59, 61, 62, 64, 66, 68, 69, 71};

#define FREQ(x) ((x)*256.0*65536/F_CPU+0.5)
// DDS-Inkremente = Additionswerte auf Phasenwert alle F_CPU/256
PROGMEM const WORD Frequencies[16] = {
 // NTSC-Trägerfrequenz (3,579545 MHz) durch:
 FREQ( 697),	// 5135,6
 FREQ( 770),	// 4648,8
 FREQ( 852),	// 4201,3
 FREQ( 941),	// 3804
 FREQ(1209),	// 2960,7
 FREQ(1336),	// 2679,3
 FREQ(1477),	// 2423,5
 FREQ(1633),	// 2192
 FREQ(1000),	// c (auf 1 kHz transponiert)
 FREQ(1122),	// d
 FREQ(1260),	// e
 FREQ(1333)};	// f (Halbtonschritt)

// Funktionsprinzip: DDS
volatile register WORD addA	asm("r8");
volatile register WORD phaA	asm("r10");	// hoher Ton
volatile register WORD addB	asm("r12");
volatile register WORD phaB	asm("r14");	// tiefer Ton

volatile register BYTE buf_r	asm("r7");   // Puffer-Lesezeiger
volatile register BYTE buf_w	asm("r6");   // Puffer-Schreibzeiger
volatile register BYTE nsi_cnt	asm("r5"); // Nummernschaltkontakt
volatile register BYTE nsi_len	asm("r4"); // Nummernschaltkontakt
volatile register BYTE ckey	asm("r3");	   // Nummernschaltkontakt
volatile register BYTE Ton	asm("r2");	   // Rest-Ton/Pausenlänge, in ~ 3 ms
#define Teiler GPIOR0			  // Software-Taktteiler
#define Flags GPIOR1			  // diverse Bits
#define Zeit8 GPIOR2			  // Zeitzähler zum Detektieren eines Erdtastendrucks

static WORD Zeit16;			    // Zeitzähler zum Abspeichern der letzten Rufnummer

static char numbers[32];		// Puffer für Wählziffern - 15 od. 20 für attiny25, 30 für attiny85
                            // sowie '*'=14, '#'=15, wie im EEPROM-Kurzwahlspeicher

/* EEPROM-Kurzwahlspeicher (256 Bytes eines ATtiny45):
11 Blöcke {
 1 Byte Nummernlänge
 30 Nibbles (15 Bytes) Ziffern (wie oben kodiert; LSN zuerst)
}
für Wahlwiederholung (Index 0) und 10 Rufnummernspeicher
5 Blöcke à 16 Bytes = 80 Bytes sind noch frei
*/

// Puffer füllen
static void PutBuf(char c) {
 asm volatile(
"	add	r30,r6	\n"
"	st	Z,%0	\n"
"	inc	r6	\n"
"	sbrc	r6,5	\n"
"	 dec	r6	\n"::"r"(c),"z"(numbers));
//numbers[buf_w++] = c;
}

// Puffer lesen, nicht aufrufen wenn leer!
// Aufruf mit freigegeben Interrupts (aus Hauptschleife) OK
static char GetBuf(void) {
 char c;
 asm volatile(
"	cli		\n"
"	add	r30,r7	\n"
"	ld	%0,Z	\n"
"	inc	r7	\n"
"	sei		\n":"=r" (c):"z" (numbers));
//c=numbers[buf_r++];
 return c;
}

static void eewait(void) {
 while (EECR&2);
}

static void eewrite(void) {
 cli();
 EECR=4;
 EECR|=2;
 sei();
}

// 12 Bytes Puffer pro Nummer (22 Ziffern)
// Beim ATtiny25 bei der Kurzwahl "0" nur 8 Bytes (14 Ziffern)
static void KurzSpeichern(BYTE idx) {
 eewait();
 EEARL=(idx<<3)+(idx<<2);	// max. 22 Stellen (24 Nibbles = 12 Bytes)
 EEDR=buf_w;
 eewrite();
 BYTE i;
 const char*z=numbers;
 for (i=0;i<buf_w;i+=2) {
  eewait();
  ++EEARL;
  EEDR=*z++<<4;	// direkt lesbar beim Lesen des EEPROMs
  EEDR|=*z++;
  eewrite();
 }
}

static void KurzLaden(BYTE idx) {
 eewait();
 EEARL=(idx<<3)+(idx<<2);
 EECR|=1;
 buf_w=EEDR;
 if (buf_w>22) buf_w=0;
 BYTE i;
 char*z=numbers;
 for (i=0;i<buf_w;i+=2) {
  ++EEARL;
  EECR|=1;
  *z++=EEDR>>4;
  *z++=EEDR&0x0F;
 }
 buf_r=0;	// mit dem Abspielen beginnen
}

static void StartTon(char z);

#ifdef FILTER
static void __attribute__((noreturn)) block(void) {
 StartTon(19);			// Ton des Todes
 for(;;) {
  Teiler=Ton=255;	// immer tönen, kein Weiterwählen
  sleep_cpu();		// keine Hauptschleife mehr
 }
}
#else
static void block(void) {}	// sollte der Compiler rauswerfen
#endif

// Nummernschaltkontakt-Auswertung, Aufruf mit F_CPU/65536 = 208 Hz
static void NSK(void) {
 BYTE key = ~PINB;
 if (key&4) {			// Erdtaste extra entprellen mit 4-Bit-Zähler
  if ((Flags&0xF0)!=0xF0) {Flags+=0x10; key&=~4;}	// Zähler noch nicht am Anschlag: Tastendruck zurücknehmen
 }else{
  if (Flags&0xF0) {Flags-=0x10; key|=4;}		// Zähler noch nicht unten: Tastendruck wiederherstellen
 }
 ckey ^= key;			// gesetztes Bit bei Änderung
 if (ckey&4) {
  if (key&4) {
   if (Flags&8) {		// Umschalten auf's Einschreiben (wie Shift+2)
    Flags&=~8;			// Wählunterdrückung weg
    Flags|=4;			// ... auch eine Wählunterdrückung
    StartTon(19);
   }
  }else{  // Erdtaste losgelassen?
   if (Flags&2) Flags&=~2;	// Hatte Shift-Funktion? Shift weg!
   else if (!(Flags&0x0C)) {
    Flags|=1;			// Sonst danach Kurzwahl
    StartTon(17);		// tönen lassen
   }
   Zeit8=0;
  }
 }
 if (key&4 && !(Flags&2)) {
  if (!++Zeit8) {
   Flags|=8;			// Kurzwahlnummer ohne zu wählen als nächstes eingeben (= Wählunterdrückung!)
   StartTon(16);
  }
 }
 if (ckey&1) {			// nsi (Nummernschalter)
  if (key&4) Flags|=2;		// Shift-Funktion entdeckt
  if (key&1) {			// nsi geschlossen!
   nsi_len = 1;			// Zeitmessung starten
  }else{			// nsi geöffnet?
   nsi_cnt++;			// Anzahl der Öffnungen zählen
  }
 }else if (key&1 && nsi_len) {	// Zeitmessung?
  nsi_len++;
  if (nsi_len>=(BYTE)(0.2*F_CPU/65536)) {	// 2 Schrittzeiten?
   if (key&4) switch (nsi_cnt) {
    case 10: buf_r=buf_w=0; KurzSpeichern(0); goto raus;	// Wahlwiederholung verhindern
    case 1: KurzLaden(0); goto raus;
    case 2: Flags|=4; StartTon(19); goto raus;
    case 3: Flags|=8; StartTon(16); goto raus;	// wie langes Drücken auf Erde
    default: nsi_cnt+=10-4;	// *,#,A,B,C,D wählen
   }else if (Flags&4) {
    KurzSpeichern(nsi_cnt);
    Flags&=~4;
    StartTon(16);
    goto raus;			// nicht wählen
   }else if (Flags&1) {
    KurzLaden(nsi_cnt);		// wählen
    Flags&=~1;
    goto raus;
   }else if (nsi_cnt==10) nsi_cnt=0;
   if (buf_w==1 && numbers[0]==0 && (nsi_cnt==0 || nsi_cnt==1)) block();	// 00 (Ausland), 01 (Sonderrufnummern, Handys)
   if (buf_w==2 && numbers[0]==0 && (numbers[1]==9 || numbers[1]==7) && nsi_cnt==0) block();	// 090 (Premium-Dienste), 070 (Rufumleitung)
   if (buf_w==2 && numbers[0]==1 && numbers[1]==1 && nsi_cnt==8) block();	// 118 (Auskunft)
   PutBuf(nsi_cnt);		// Zählergebnis abspeichern
   if (Flags&8) {
    buf_r=buf_w;		// Nicht wählen
    StartTon(18);
   }
   else Zeit16=5*F_CPU/65536;	// Wahlwiederholungs-Abspeicher-TimeOut setzen (5 Sekunden)
raus:
   nsi_len = nsi_cnt = 0;
  }
 }
 ckey = key;
}

// Aufruf mit 20 MHz/256 = 78,125 kHz
ISR(TIM0_OVF_vect) {
// Tonlänge nominell 70 ms, Pause 30 ms
// Bei 20 MHz sind das 5469 bzw. 2344 ISR-Aufrufe.
// Durch 256 geteilt sind's handliche 21 bzw. 9.
// Ton>=0 = Tonausgabe, sonst Pause
 if (Ton>=(BYTE)(PAUSE*F_CPU/65536)) {
#if 0
  BYTE a=pgm_read_byte(SinTab+((phaA+=addA)>>8));
  BYTE b=pgm_read_byte(SinTab+((phaB+=addB)>>8));
  OCR1A=a+b-(b>>2);		// hier ohne Runden
#else
  asm(
"	add	r10,r8	\n"
"	adc	r11,r9	\n"
"	add	%A1,r11	\n"
"	adc	%B1,r1	\n"
"	lpm		\n"
"	sub	%A1,r11	\n"	// ZH:ZL wiederherstellen
"	sbc	%B1,r1	\n"
"	add	r14,r12	\n"
"	adc	r15,r13	\n"
"	add	%A1,r15	\n"
"	adc	%B1,r1	\n"
"	lpm	r1,z	\n"	// gcc compiliert fäschlicherweise "lpm r1,Z+"
"	add	r0,r1	\n"
"	lsr	r1	\n"	// (kompensiert Kabeldämpfung für höhere Frequenzen)
"	lsr	r1	\n"
"	sbc	r0,r1	\n"	// "sbc" ist der Trick fürs Abrunden
"	clr	r1	\n"	// __zero_reg__ wiederherstellen
"	out	%0,r0	\n"
  ::"I" (_SFR_IO_ADDR(OCR1A)),"z"(SinTab):"1");
#endif
 }
 if (CLKPR || !--Teiler) {
  asm volatile("rcall __vector_1");	// kein ((void(*)(void))__vector)(), denn das führt zum weiträumigen Register-Push
 }
}

ISR(_VECTOR(1),ISR_NOBLOCK) {
 if (Ton) --Ton;
 NSK();		// Nummernschaltkontakt-Auswertung
 if (Zeit16 && !--Zeit16) KurzSpeichern(0);
}

PROGMEM const BYTE Nr2HiLo[20]={
//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    *    #   ..Hinweistöne..
 0x53,0x40,0x50,0x60,0x41,0x51,0x61,0x42,0x52,0x62,0x70,0x71,0x72,0x73,0x43,0x63,0x8F,0x9F,0xAF,0xBF};
// High-Nibble  4  5  6  7
// Low-Nibble ┌───────────
//	    0 │ 1  2  3  A
//	    1 │ 4  5  6  B
//	    2 │ 7  8  9  C
//	    3 │ *  0  #  D

// Startet Ton für Ziffer z ("*"=14, "#"=15)
static void StartTon(char z) {
 if ((BYTE)z>=20) return;	// sollte nie vorkommen
 if (CLKPR) {
  cli();
  CLKPR=0x80;
  CLKPR=0;
  sei();
 }
 BYTE i=pgm_read_byte(Nr2HiLo+z);		// umrechnen in die 2 Frequenzen
 addA=pgm_read_word(Frequencies+(i>>4));	// hoher Ton im High-Nibble
 addB=pgm_read_word(Frequencies+(i&15));	// tiefer Ton im Low-Nibble
 if (!PLLCSR) {
  PLLCSR|=0x02;
  _delay_us(100);
  while (!(PLLCSR&1));
  PLLCSR|=0x04;
  TCCR1=0x61;
 }
 Ton=(TON+PAUSE)*F_CPU/65536;			// Tonausgabe in ISR starten
 DDRB|=0x02;					// Ausgang aktivieren
}

/*************************************
 * Initialisierung und Hauptschleife *
 *************************************/

static void hardwareInit(void) {
 CLKPR = 0x80;
 CLKPR = 0x08;		// Taktteiler /256 (~ 64 kHz)
 ACSR |= 0x80;		// Analogvergleicher ausschalten
 MCUCR = 0x20;		// Sleep-Modus: Idle (Timer läuft)
 PORTB = 0x05;		// Pullups für 2 Eingänge
 DIDR0 = 0x3A;		// diese digitalen Eingänge nicht nutzen
 TCCR0B= 0x01;		// Timer0: Vorteiler 1
 TIMSK = 0x02;		// Überlauf-Interrupt
 ckey = ~PINB;		// einlesen
 nsi_len = nsi_cnt = 0;	// alle übrigen Register nullsetzen
 buf_w = buf_r = 0;
 Ton = 0;
 Flags = Zeit8 = 0;
}

int __attribute__((noreturn)) main(void) {
 hardwareInit();
slow:
 DDRB&=~0x02;			// Ausgang hochohmig
 TCCR1=0;			// Timer1 aus
 PLLCSR=0;			// PLL aus
 cli();
 CLKPR=0x80;
 CLKPR=0x08;			// Taktteiler /256 (~ 64 kHz)
 sei();
 for(;;) {		// Hauptschleife
  sleep_cpu();
  if (Ton) continue;		// Ton- oder Pausenausgabe läuft
  if (buf_r==buf_w) goto slow;	// Puffer leer, nichts zu tun
  StartTon(GetBuf());		// Nächsten Ton + Pause ausgeben
 }
}

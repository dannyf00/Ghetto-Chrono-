#include "led4_pins.h"				  //we use 4-digit 7-segment leds

//hardware configuration
//#define PORTB			PORTB
//#define PORTA			PORTA
//digit definitions
#if 0					//led display on top of the IC, left to right orientation (ic pin1=led pin1)
#define DIG1_PORT		PORTC
#define DIG1_DDR		DDRC
#define DIG1			(1<<5)

#define DIG2_PORT		PORTC
#define DIG2_DDR		DDRC
#define DIG2			(1<<2)

#define DIG3_PORT		PORTC
#define DIG3_DDR		DDRC
#define DIG3			(1<<1)

#define DIG4_PORT		PORTD
#define DIG4_DDR		DDRD
#define DIG4			(1<<4)

//segment definitions
//map 4-digit led display to pic16f1936 direction
#define SEGA_PORT		PORTC
#define SEGA_DDR		DDRC
#define SEGA			(1<<4)

#define SEGB_PORT		PORTC
#define SEGB_DDR		DDRC
#define SEGB			(1<<0)

#define SEGC_PORT		PORTD
#define SEGC_DDR		DDRD
#define SEGC			(1<<2)

#define SEGD_PORT		PORTD
#define SEGD_DDR		DDRD
#define SEGD			(1<<0)

#define SEGE_PORT		PORTD
#define SEGE_DDR		DDRD
#define SEGE			(1<<5)

#define SEGF_PORT		PORTC
#define SEGF_DDR		DDRC
#define SEGF			(1<<3)

#define SEGG_PORT		PORTD
#define SEGG_DDR		DDRD
#define SEGG			(1<<3)

#define SEGDP_PORT		PORTD
#define SEGDP_DDR		DDRD
#define SEGDP			(1<<1)

#else					//led on top of the ic, right to left orientation (IC pin1=led pin7)

#define DIG1_PORT		PORTD
#define DIG1_DDR		DDRD
#define DIG1			(1<<4)

#define DIG2_PORT		PORTD
#define DIG2_DDR		DDRD
#define DIG2			(1<<1)

#define DIG3_PORT		PORTD
#define DIG3_DDR		DDRD
#define DIG3			(1<<0)

#define DIG4_PORT		PORTC
#define DIG4_DDR		DDRC
#define DIG4			(1<<5)

//segment definitions
//map 4-digit led display to pic16f1936 direction
#define SEGA_PORT		PORTD
#define SEGA_DDR		DDRD
#define SEGA			(1<<3)

#define SEGB_PORT		PORTD
#define SEGB_DDR		DDRD
#define SEGB			(1<<5)

#define SEGC_PORT		PORTC
#define SEGC_DDR		DDRC
#define SEGC			(1<<3)

#define SEGD_PORT		PORTC
#define SEGD_DDR		DDRC
#define SEGD			(1<<1)

#define SEGE_PORT		PORTC
#define SEGE_DDR		DDRC
#define SEGE			(1<<0)

#define SEGF_PORT		PORTD
#define SEGF_DDR		DDRD
#define SEGF			(1<<2)

#define SEGG_PORT		PORTC
#define SEGG_DDR		DDRC
#define SEGG			(1<<4)

#define SEGDP_PORT		PORTC
#define SEGDP_DDR		DDRC
#define SEGDP			(1<<2)
#endif
//end hardware configuration

//digit control - active high (Common Anode) or active low (Common Cathode)
#define DIG_ON(port, pins)		IO_SET(port, pins)			//turn on a digit
#define DIG_OFF(port, pins)		IO_CLR(port, pins)			//turn off a digit

//segment control - active low (Common Anode) or active high (Common Cathode)
#define SEG_ON(port, pins)		IO_CLR(port, pins)			//turn on a segment
#define SEG_OFF(port, pins)		IO_SET(port, pins)			//turn off a segment

//global defines

//global variables
unsigned char lRAM[4];				//led display buffer
//led font.
//SEGDP = 0x80
//SEGG   = 0x40
//SEGF   = 0x20
//SEGE   = 0x10
//SEGD   = 0x08
//SEGC   = 0x04
//SEGB   = 0x02
//SEGA   = 0x01
//led font for numerical display '0'..'9''a'..'f', active high
const unsigned char ledfont_num[]={		//led font, for common anode
	0x3f,								//'0'
	0x06,								//'1'
	0x5b,								//'2'
	0x4f,								//'3'
	0x66,								//'4'
	0x6d,								//'5'
	0x7d,								//'6'
	0x07,								//'7'
	0x7f,								//'8'
	0x6f,								//'9'
	0x5f,								//'a'
	0x7c,								//'b'
	0x58,								//'c'
	0x5e,								//'d'
	0x79,								//'e'
	0x71,								//'f'
	0x00								//' ' blank
};
//led font for alphabetic display 'a'..'z'
const unsigned char ledfont_alpha[]={		//led font, for common anode
	0x5f,								//'a'
	0x7c,								//'b'
	0x58,								//'c'
	0x5e,								//'d'
	0x79,								//'e'
	0x71,								//'f'
	0x6f,								//'g'
	0x07,								//'h'
	0x74,								//'i'
	0x0e,								//'j'
	0x00,								//'k'
	0x38,								//'l'
	0x00,								//'m'
	0x54,								//'n'
	0x5c,								//'o'
	0x73,								//'p'
    0x67,								//'q'
    0x77,								//'r'
    0x6d,								//'s'
    0x00,								//'t'
    0x1c,								//'u'
    0x00,								//'v'
    0x00,								//'w'
    0x00,								//'x'
    0x6e,								//'y'
    0x00,								//'z'
	0x00								//' ' blank
};

//initialize the pins
void led_init(void) {
	//turn off the digits send set pins to output
	DIG_OFF(DIG1_PORT, DIG1); IO_OUT(DIG1_DDR, DIG1);
	DIG_OFF(DIG2_PORT, DIG2); IO_OUT(DIG2_DDR, DIG2);
	DIG_OFF(DIG3_PORT, DIG3); IO_OUT(DIG3_DDR, DIG3);
	DIG_OFF(DIG4_PORT, DIG4); IO_OUT(DIG4_DDR, DIG4);

	//turn off the segments
	SEG_OFF(SEGA_PORT, SEGA); IO_OUT(SEGA_DDR, SEGA);
	SEG_OFF(SEGB_PORT, SEGB); IO_OUT(SEGB_DDR, SEGB);
	SEG_OFF(SEGC_PORT, SEGC); IO_OUT(SEGC_DDR, SEGC);
	SEG_OFF(SEGD_PORT, SEGD); IO_OUT(SEGD_DDR, SEGD);
	SEG_OFF(SEGE_PORT, SEGE); IO_OUT(SEGE_DDR, SEGE);
	SEG_OFF(SEGF_PORT, SEGF); IO_OUT(SEGF_DDR, SEGF);
	SEG_OFF(SEGG_PORT, SEGG); IO_OUT(SEGG_DDR, SEGG);
	SEG_OFF(SEGDP_PORT, SEGDP); IO_OUT(SEGDP_DDR, SEGDP);

	//set all pins to output
	//put your code here
}

//display the ledram
void led_display(void) {
	static unsigned char dig=0;		//current digit
	unsigned char tmp;

	//turn off the digits
	DIG_OFF(DIG1_PORT, DIG1);
	DIG_OFF(DIG2_PORT, DIG2);
	DIG_OFF(DIG3_PORT, DIG3);
	DIG_OFF(DIG4_PORT, DIG4);

	//tmp=ledfont_num[lRAM[dig]];					//retrieve font / segment info from the display buffer
	tmp = lRAM[dig];							//alternative: if user fills the display buffer lRAM[] with segment information
	//turn on/off the segments
	if (tmp & 0x01) SEG_ON(SEGA_PORT, SEGA); else SEG_OFF(SEGA_PORT, SEGA);
	if (tmp & 0x02) SEG_ON(SEGB_PORT, SEGB); else SEG_OFF(SEGB_PORT, SEGB);
	if (tmp & 0x04) SEG_ON(SEGC_PORT, SEGC); else SEG_OFF(SEGC_PORT, SEGC);
	if (tmp & 0x08) SEG_ON(SEGD_PORT, SEGD); else SEG_OFF(SEGD_PORT, SEGD);
	if (tmp & 0x10) SEG_ON(SEGE_PORT, SEGE); else SEG_OFF(SEGE_PORT, SEGE);
	if (tmp & 0x20) SEG_ON(SEGF_PORT, SEGF); else SEG_OFF(SEGF_PORT, SEGF);
	if (tmp & 0x40) SEG_ON(SEGG_PORT, SEGG); else SEG_OFF(SEGG_PORT, SEGG);
	if (tmp & 0x80) SEG_ON(SEGDP_PORT, SEGDP); else SEG_OFF(SEGDP_PORT, SEGDP);

	//turn on the digit and advance to the next digit
	switch (dig) {
		case 0: DIG_ON(DIG1_PORT, DIG1); dig=1; break;
		case 1: DIG_ON(DIG2_PORT, DIG2); dig=2; break;
		case 2: DIG_ON(DIG3_PORT, DIG3); dig=3; break;
		case 3: DIG_ON(DIG4_PORT, DIG4); dig=0; break;
	}
}


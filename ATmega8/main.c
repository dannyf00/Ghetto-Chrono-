//#include <avr/io.h>							//we use gcc-avr
#include "gpio.h"
#include "delay.h"							//we use software delays
#include "led4_pins.h"						//we use 4-digit led display - different wiring!

//hardware configuration
#define CHRONO_PORT				PORTB
#define CHRONO_DDR				DDRB
#define CHRONO					(1<<0)		//ICP1 on PB0

#define CHRONO_PS				TMR1PS_1x	//tmr1 prescaler
#define CHRONO_DISTANCE			1234		//chrono sensor distance, x10mm (1234=123.4mm)
#define CHRONO_TRIGGER			RISING		//input capture on rising / falling edge
//end hardware configuration

//global defines
#define RISING					0
#define FALLING					1
#define TMR1PS_1x				0x01		//0x01->1x prescaler
#define TMR1PS_8x				0x02		//0x02->8x prescaler
#define TMR1PS_64x				0x03		//0x03->64x prescaler
#define TMR1PS_256x				0x04		//0x04->256x prescaler
#define TMR1PS_1024x			0x05		//0x05->1024x prescaler

//conversion routines
//converting ticks to us
uint32_t ticks2us(uint32_t ticks) {
	switch (TCCR1B & 0x07) {
		case TMR1PS_1x: ticks *= 1; break;	//1x prescaler
		case TMR1PS_8x: ticks *= 8; break;	//8x prescaler
		case TMR1PS_64x: ticks *= 64; break;
		case TMR1PS_256x: ticks *= 256; break;
		case TMR1PS_1024x: ticks *= 1024; break;
	}
	return ticks / (F_CPU / 1000000ul);		//return us
}

//convert ticks to mpsx10 using floating point math
uint32_t ticks2mpsx10_fp(uint32_t ticks) {
	//return (float) CHRONO_DISTANCE * 0.001 / ((float) ticks2us(ticks) * 1e-6);
	return (float) CHRONO_DISTANCE * 1000.0 / (float) ticks2us(ticks);
}

//convert ticks to meters per second x 10 (mpsx10) using integer math
uint32_t ticks2mpsx10(uint32_t ticks) {
	uint32_t tmp = (uint32_t) CHRONO_DISTANCE * 1000ul * (F_CPU / 1000000ul) / ticks;
	switch (TCCR1B & 0x07) {
		case TMR1PS_1x: tmp /= 1; break;
		case TMR1PS_8x: tmp /= 8; break;
		case TMR1PS_64x: tmp /= 64; break;
		case TMR1PS_256x: tmp /= 256; break;
		case TMR1PS_1024x: tmp /= 1024; break;
	};
	return tmp;
}

//convert ticks to ft per second x 10 (fpsx10) using integer math
uint32_t ticks2fpsx10(uint32_t ticks) {
	return ticks2mpsx10(ticks) * 32804ul/10000;			//1meter = 3.28084
}

//global variables
volatile uint16_t chrono_ticks=0;			//ticks elapsed between start / end
volatile char chrono_available=0;			//data availability flag. 1=new data available, 0=no new data available

//tmr1 capture isr
ISR(TIMER1_CAPT_vect) {
	static uint16_t chrono_start, chrono_end;
	static char chrono_ch=0;				//chrono input channel. 0=>put ICR1 into start, 1->put ICR1 input end

	//clear the flag -> done automatically
	if (chrono_ch++ & 0x01) {				//ch = 0 right now -> ICR1 to start
		chrono_start = ICR1;				//save ICR1 to chrono_start
	} else {								//ch = 1 right now -> ICR1 to end
		chrono_end = ICR1;					//save ICR1 to end
		chrono_ticks = chrono_end - chrono_start;	//calculate ticks elapsed
		chrono_available = 1;				//1->new data available
	}
}

//reset the chrono
void chrono_init(void) {
	//reset chrono variables
	chrono_ticks = 0;
	chrono_available = 0;					//no new data

	//chrono input pin as input
	IO_IN(CHRONO_DDR, CHRONO);

	//configure tmr1 as free-running, 1x prescaler
	//TCCR1A = TCCR1B = 0;
	TCCR1B = (TCCR1B & ~0x07) | (0x00 & 0x07);	//stop tmr1
	//put tmr1 in normal mode (WGM13..0 = 0b0000)
	TCCR1A = (TCCR1A & ~0x03) | (0x00 & 0x03);
	TCCR1B = (TCCR1B & ~0x18) | (0x00 & 0x18);

	//configure input capture on ICP1/PB0
	//disable noise filter -> capture on the first edge
	TCCR1B = (TCCR1B & ~0x80) | (0x00 & 0x80);
#if CHRONO_TRIGGER == RISING
	TCCR1B = (TCCR1B & ~0x40) | (0x40 & 0x40);	//1->rising edge
#else
	TCCR1B = (TCCR1B & ~0x40) | (0x00 & 0x40);	//0->falling edge
#endif

	//enable tmr1 input capture interrupt
	TIFR |= (1<<ICF1);						//1->clear the flag
	TIMSK |= (1<<TICIE1);					//1->enable the input capture interrupt

	//start tmr1
	TCCR1B = (TCCR1B & ~0x07) | (CHRONO_PS & 0x07);	//start timer on 1x prescaler
}

int main(void) {
	uint16_t tmp;							//number to be displayed
	char tmp1, dot;

	mcu_init();								//reset the mcu
	led_init();								//reset the led
	chrono_init();							//reset the chrono

	ei();									//enable global interrupt

	while(1) {
#if 0										//for debugging only
		//force an input trigger on ICP1/CHRONO
		IO_SET(CHRONO_PORT, CHRONO); IO_CLR(CHRONO_PORT, CHRONO);
		//first pulse is triggered, ICR1 into chrono_start
		//do something here
		//tmp=ticks2mpsx10_fp(100);
		//delay_ms(10);							//19 ticks
		IO_SET(CHRONO_PORT, CHRONO); IO_CLR(CHRONO_PORT, CHRONO);
		//second pulse is triggered, ICR1 into chrono_start
#endif

		//convert chrono_ticks only if the data is new
		if (chrono_available) {
			chrono_available = 0;
			//pick the variable to display
			tmp = chrono_ticks;
			//chrono_ticks = 1000;					//for debugging only
			//tmp = ticks2us(chrono_ticks);			//1000 ticks@8Mhz -> 125us
			//tmp = ticks2mpsx10_fp(chrono_ticks) / 10;		//123.4mm/125us=987.2, displayed as 987.2. very minor flickering at 1Mhz
			//tmp = (ticks2mpsx10(chrono_ticks) + 5)/ 10;		//123.4mm/125us=987.2, displayed as 987. no flickering at 1Mhz. with rouding.
			//tmp = (ticks2fpsx10(chrono_ticks) + 5)/ 10;		//987.2mps->3238.845, displayed as 3238. no flickering at 1Mhz. with rouding.
			if (tmp >= 9999) tmp = 9999;			//bound tmp
			//display tmp
#if 0		//slower display routine
			lRAM[3]=(tmp % 10) + 0; tmp /= 10;
			lRAM[2]=(tmp % 10) + 0; tmp /= 10;
			lRAM[1]=(tmp % 10) + 0; tmp /= 10;
			lRAM[0]=(tmp % 10) + 0; tmp /= 10;
#else		//faster display routine
			tmp1=0; while (tmp >= 1000) {tmp -=1000; tmp1+=1;}; lRAM[0]=tmp1;
			tmp1=0; while (tmp >=  100) {tmp -= 100; tmp1+=1;}; lRAM[1]=tmp1;
			tmp1=0; while (tmp >=   10) {tmp -=  10; tmp1+=1;}; lRAM[2]=tmp1;
			/*tmp1=0; while (tmp >= 0001) {tmp -=0001; tmp1+=1;}; */lRAM[3]=tmp;
#endif
			//if (tmp >= DISPLAY_MAX) tmp = DISPLAY_MAX;		//bound tmp
			//tmp1=0; while (tmp >= 100) {tmp -= 100; tmp1+=1;} lRAM[0]=tmp1;
			//tmp1=0; while (tmp >=  10) {tmp -=  10; tmp1+=1;} lRAM[1]=tmp1;
			///*tmp1=0; while (tmp >= 100) {tmp -= 100; tmp1+=1;} */lRAM[2]=tmp;
		}

		//blanking here if needed
		led_display();						//display lRAM[]
	}

	return 0;
}

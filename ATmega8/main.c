//Ghetto Chrono on ATmega8/8L
//Compiler: GCC-AVR, O1
//version 0.1, 9/29/2017

#include "gpio.h"
#include "delay.h"							//we use software delays
#include "led4_pins.h"						//we use 4-digit led display - different wiring!

//hardware configuration
#define CHRONO_PORT				PORTB
#define CHRONO_DDR				DDRB
#define CHRONO					(1<<0)		//ICP1 on PB0

//status indicators - active high
//status:
//both indicators on: ready to receive data
//LED_START on: both _start and _stop measurements have been taken. ready to take the next measurements.
//LED_START off: first measurement has been taken, but second measurement not yet
//if LED_START is stuck in "off", reset the chrono
#define LED_PORT				PORTB
#define LED_DDR					DDRB
#define LED_START				(1<<1)		//start led on PB1
#define LED_STOP				(0<<2)		//stop led on PB? - not used

#define CHRONO_PS				TMR1PS_1x	//tmr1 prescaler
#define CHRONO_DISTANCE			1234		//chrono sensor distance, x10mm (1234=123.4mm)
#define CHRONO_TRIGGER			RISING		//input capture on rising / falling edge
#define CHRONO_DP							//define CHRONO_DP if you want to show decimal point on digit 3/4.
//#define FAST_MATH							//using faster math so the code runs at 1Mhz

#define OSCCAL_CAL				0xbd		//0xbd@1mhz, 0xbf@2mhz, 0xbd@4Mhz, 0xcd@8Mhz. Device and frequency specific (b3 b2 ae ae)

//debug_pin used to generate a pulse to trigger ICP1
#define DEBUG_PORT				PORTB
#define DEBUG_DDR				DDRB
//#define DEBUG_PIN				(1<<2)		//comment out if not used
//end hardware configuration

//global defines
#define RISING					0
#define FALLING					1
#define TMR1PS_1x				0x01		//0x01->1x prescaler
#define TMR1PS_8x				0x02		//0x02->8x prescaler
#define TMR1PS_64x				0x03		//0x03->64x prescaler
#define TMR1PS_256x				0x04		//0x04->256x prescaler
#define TMR1PS_1024x			0x05		//0x05->1024x prescaler

//led indicators - active high
#define LED_ON(LEDs)			IO_SET(LED_PORT, LEDs)
#define LED_OFF(LEDs)			IO_CLR(LED_PORT, LEDs)

//global variables
volatile uint16_t chrono_ticks=0;			//ticks elapsed between start / end
volatile char chrono_available=0;			//data availability flag. 1=new data available, 0=no new data available

//conversion routines
//converting ticks to us
uint32_t ticks2usx10(uint32_t ticks) {
	switch (TCCR1B & 0x07) {
		case TMR1PS_1x: ticks *= 1; break;	//1x prescaler
		case TMR1PS_8x: ticks *= 8; break;	//8x prescaler
		case TMR1PS_64x: ticks *= 64; break;
		case TMR1PS_256x: ticks *= 256; break;
		case TMR1PS_1024x: ticks *= 1024; break;
	}
	return ticks / (F_CPU / 1000000ul) * 10;		//return us
}

//convert ticks to mpsx10 using floating point math
//for demo only, not used -> too slow / bulky
uint32_t ticks2mpsx10_fp(uint32_t ticks) {
	//return (float) CHRONO_DISTANCE * 0.001 / ((float) ticks2us(ticks) * 1e-6);
	//return (float) CHRONO_DISTANCE * 1000.0 * 10.0 / (float) ticks2usx10(ticks);
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

//tmr1 capture isr
ISR(TIMER1_CAPT_vect) {
	static uint16_t chrono_start, chrono_end;
	static char chrono_ch=0;				//chrono input channel. 0=>put ICR1 into start, 1->put ICR1 input end

	//clear the flag -> done automatically
	if ((chrono_ch++ & 0x01)==0) {			//ch = 0 right now -> ICR1 to start
		chrono_start = ICR1;				//save ICR1 to chrono_start
		LED_OFF(LED_START);					//turn off the start led
	} else {								//ch = 1 right now -> ICR1 to end
		chrono_end = ICR1;					//save ICR1 to end
		chrono_ticks = chrono_end - chrono_start;	//calculate ticks elapsed
		chrono_available = 1;				//1->new data available
		LED_OFF(LED_STOP); 					//turn off the stop led
	}
}

//reset the chrono
//tmr1 free running, no overflow interrupt
//ICP1 at 1x sampling.
void chrono_init(void) {
	//reset chrono variables
	chrono_ticks = 0;
	chrono_available = 0;					//no new data

	//set up the indicators
	//led_start / _stop as output, on
	LED_ON(LED_START | LED_STOP);
	IO_OUT(LED_DDR, LED_START | LED_STOP);

	//chrono input pin as input, with pull-up enabled
	IO_IN(CHRONO_DDR, CHRONO); 				//pin as input
	IO_SET(CHRONO_PORT, CHRONO);			//enable pull-up

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
	uint32_t tmp;							//number to be displayed
	char tmp1, dp;							//dp = decimal point, =2(digit 3) or 3(digit 4)
	uint16_t cnt=0;							//counter

	mcu_init();								//reset the mcu

#if defined(OSCCAL_CAL)
	OSCCAL = OSCCAL_CAL;					//calibration for Internal RC oscillator
#endif
	led_init();								//reset the led
	chrono_init();							//reset the chrono

#if defined(DEBUG_PIN)
	IO_OUT(DEBUG_DDR, DEBUG_PIN);
#endif

	ei();									//enable global interrupt
	while(1) {
#if defined(DEBUG_PIN)						//for debugging only
		//force an input trigger on ICP1/CHRONO
		//simulated input, if CHRONO is configured as output
		//IO_FLP(DEBUG_PORT, DEBUG_PIN);
		IO_SET(DEBUG_PORT, DEBUG_PIN); IO_CLR(DEBUG_PORT, DEBUG_PIN);
		//first pulse is triggered, ICR1 into chrono_start
		//do something here
		//tmp=ticks2mpsx10_fp(100);
		delay(10);							//884 ticks
		IO_SET(DEBUG_PORT, DEBUG_PIN); IO_CLR(DEBUG_PORT, DEBUG_PIN);
		//second pulse is triggered, ICR1 into chrono_start
#endif


		//convert chrono_ticks only if the data is new
		//chrono_available=1;
		if (chrono_available) {
			chrono_available = 0;
			//chrono_ticks = 1000;								//for debugging only - to make sure that the math is correct
			//pick the variable to display
			tmp = chrono_ticks;
			//tmp = ticks2usx10(chrono_ticks);					//1000 ticks@8Mhz -> 125us
			//tmp = ticks2mpsx10_fp(chrono_ticks);				//123.4mm/125us=987.2, displayed as 987.2. very minor flickering at 1Mhz
			//tmp = ticks2mpsx10(chrono_ticks);					//123.4mm/125us=987.2, displayed as 987. no flickering at 1Mhz. with rouding.
			//tmp = ticks2fpsx10(chrono_ticks);					//987.2mps->3238.845, displayed as 3238. no flickering at 1Mhz. with rouding.
			if (tmp > 99999 - 5) {tmp = 99999 - 5;}				//bound tmp, dp on digit 4. "5" here for rounding
#if defined(CHRONO_DP)
			//decide where the decimal point should be, digit 3 or digit 4
			//allows for rounding
			if (tmp > 9999) {tmp = (tmp + 5) / 10; dp = 3;} else {tmp /= 1; dp = 2;}	//only two decimal points are displayed
#else
			tmp = (tmp + 5) / 10;								//4 digits only, rounding applied. "/10" due to speed measurements being x10.
#endif
			//display tmp by forming the string in display buffer lRAM[]
#ifdef FAST_MATH
			//faster display routine - no flicker at 1Mhz
			tmp1=0; while (tmp >= 1000) {tmp -=1000; tmp1+=1;}; lRAM[0]=tmp1; lRAM[0]=ledfont_num[tmp1];
			tmp1=0; while (tmp >=  100) {tmp -= 100; tmp1+=1;}; lRAM[1]=tmp1; lRAM[1]=ledfont_num[tmp1];
			tmp1=0; while (tmp >=   10) {tmp -=  10; tmp1+=1;}; lRAM[2]=tmp1; lRAM[2]=ledfont_num[tmp1];
			/*tmp1=0; while (tmp >= 0001) {tmp -=0001; tmp1+=1;}; */lRAM[3]=tmp; lRAM[3]=ledfont_num[tmp];
#else
			//slower display routine - slight flicker at 1Mhz
			lRAM[3]=ledfont_num[(tmp % 10) + 0]; tmp /= 10;
			lRAM[2]=ledfont_num[(tmp % 10) + 0]; tmp /= 10;
			lRAM[1]=ledfont_num[(tmp % 10) + 0]; tmp /= 10;
			lRAM[0]=ledfont_num[(tmp % 10) + 0]; tmp /= 10;
#endif
#if defined(CHRONO_DP)
			//display the decimal point
			switch (dp) {
				case 2: lRAM[2]|=0x80; break;					//dp on digit 3
				case 3: lRAM[3]|=0x80; break;					//decimal point on digit 4
			}
#endif
			LED_ON(LED_START | LED_STOP);						//turn on both leds to indicate ready to fire status
		}

		//blanking here if needed
		led_display();						//display lRAM[]
	}

	return 0;
}

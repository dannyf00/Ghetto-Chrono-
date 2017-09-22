#include "config.h"						//configuration words
#include "gpio.h"                           //we use gpio functions
#include "delay.h"                          //we use software delays
#include "led4_pins.h"						//led display routines
#include "tmr0.h"							//driving led - not used
#include "tmr1.h"							//chrono timer -> configured as systick timer


//hardware configuration
#define CHRONO_DLY				1			//delay
#define CHRONO_START			(1<<0)		//chrono starts on rising edge of PB0
#define CHRONO_END				(1<<7)		//chrono ends on rising edge of PB7
#define CHRONO_TRIGGER			FALLING		//chrono-trigger: RISING/FALLING
#define systicks()				TMR1		//systicks mapped to TMR1 -> short overflow

#define RISING					0
#define FALLING					1
//end hardware configuration

//global defines

//global variables
//volatile uint16_t systicks_msw=0;			//16-bit systick msw
//volatile uint32_t systicks=0;				//systicks
//char lRAM[4];								//display buffer - declared in led4_pins
volatile uint16_t chrono_ticks=0;
volatile char chrono_available=0;			//1=new data available in chrono_ticks, 0=no new data

//prototypes
//uint32_t systicks(void);

//global isr
void interrupt isr(void) {
	static uint16_t ticks_start, ticks_end;
	//tmr1 isr
	//if (TMR1IF) {
	//	TMR1IF = 0;								//clear the flag
	//	systicks_msw+=1;						//advance the msw
	//}	
	
	//IOC interrupt isr
	if (IOCIF) {
		IOCIF = 0;							//clera the flag
		if (IOCBF & CHRONO_START) {			//chrono to start
			IOCBF ^= CHRONO_START;			//clear the flag
			ticks_start = systicks();		//time stamp ticks_start
		}
		
		if (IOCBF & CHRONO_END) {			//chrono to end
			IOCBF ^= CHRONO_END;			//clear the flag
			ticks_end = systicks();			//time stamp ticks_end
			chrono_ticks = ticks_end - ticks_start;	//calculate time elapsed
			chrono_available = 1;			//1=new data available
		}
	}		
}

#if 0
//get systicks
uint32_t systicks(void) {
	uint16_t m;								//msw
	uint16_t f;								//timer count
	
	m = systicks_msw;						//read the msw
	do {
		f = TMR1;							//read the timer
	} while (systicks_msw == m);			//make sure that no overflow has taken place / for atomicity
	return (((uint32_t) m) << 16) | f;
}
#endif

//initialize the chrono
void chrono_init(void) {
	//no new data
	chrono_available = 0;					//no new data available
	
	//set up tmr1
	tmr1_init(TMR1_PS1x, 0);				//configured as free-running 16-bit timer @ 1x prescaler
	//tmr1_act(systick_isr);					//systick handler
	TMR1IF = 0;								//clear the flag
	TMR1IE = 0;								//tmr1 interrupt off -> max timing is 0xffff* prescaler
	//tmr1 is now running freely
	
	//set up external interrupts
#if CHRONO_TRIGGER == RISING
	IO_SET(IOCBP, CHRONO_START | CHRONO_END); IO_CLR(IOCBN, CHRONO_START | CHRONO_END);		//trigger on positive edge on chrono_start and _end pins
#else
	IO_CLR(IOCBP, CHRONO_START | CHRONO_END); IO_SET(IOCBN, CHRONO_START | CHRONO_END);		//trigger on negative edge on chrono_start and _end pins
#endif
	IOCIF = 0;								//clear the flag
	IOCIE = 1;								//enable the isr

	PEIE = 1;								//peripheral interrupt on
}

int main(void) {
	uint16_t tmp;							//4-digit display variable
	
	mcu_init();							    //initialize the mcu, 16Mhz
	
	//set up led display
	led_init();								//reset the led
	chrono_init();							//reset the chrono
	
	ei();									//enable global interrupts
	while (1) {
		if (chrono_available) {
			chrono_available = 0;			//reset the flag
			tmp = 1234;							//increment tmp
			//display tmp
			//format lRAM[4]
			lRAM[3]=(tmp % 10) + '0'; tmp /= 10;
			lRAM[2]=(tmp % 10) + '0'; tmp /= 10;
			lRAM[1]=(tmp % 10) + '0'; tmp /= 10;
			lRAM[0]=(tmp % 10) + '0'; tmp /= 10;
			//blank leading zero here if you want
			led_display();						//update the display
		}	
		//delay_ms(CHRONO_DLY);				//waste some time
	}
}


//"ghetto chronometer":
//using 16-bit timer1 as counter
//timer capture to measure time elapsed

#include "config.h"						//configuration words
#include "gpio.h"                           //we use gpio functions
#include "delay.h"                          //we use software delays
#include "led4_pins.h"						//led display routines
#include "tmr0.h"							//driving led - not used
#include "tmr1.h"							//chrono timer -> configured as systick timer


//hardware configuration
#define CHRONO_DLY				1			//delay
#define CHRONO_DDR				TRISC		//RC2/CCP1/CHRONO_START, RC1/CCP2/CHRONO_STOP
#define CHRONO_START			(1<<2)		//RC2/CCP1/CHRONO_START
#define CHRONO_STOP				(1<<1)		//RC1/CCP2/CHRONO_STOP
#define CHRONO_TRIGGER			RISING		//chrono-trigger: RISING/FALLING
#define systicks()				(TMR1)		//systicks mapped to TMR1 -> short overflow

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
	static uint16_t chrono_start, chrono_stop;
	//tmr1 isr
	//if (TMR1IF) {
	//	TMR1IF = 0;								//clear the flag
	//	systicks_msw+=1;						//advance the msw
	//}	
	
	//CCP interrupt isr
	if (CCP1IF) {
		CCP1IF = 0;							//clear the flag
		chrono_start = CCPR1;				//systicks();			//record the timebase
	}
	
	if (CCP2IF) {
		CCP2IF = 0;							//clear the flag
		chrono_stop = CCPR2;				//systicks();			//record the time base		
		chrono_ticks = chrono_stop - chrono_start;	//calculate time elapsed
		chrono_available = 1;				//1=new data available
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
	//initialize the CHRONO_START/_STOP as input pins
	IO_IN(CHRONO_DDR, CHRONO_START | CHRONO_STOP);
	
	//no new data
	chrono_available = 0;					//no new data available
	
	//set up tmr1
	tmr1_init(TMR1_PS1x, 0);				//configured as free-running 16-bit timer @ 1x prescaler
	//tmr1_act(systick_isr);					//systick handler
	TMR1IF = 0;								//clear the flag
	TMR1IE = 0;								//tmr1 interrupt off -> max timing is 0xffff* prescaler
	//tmr1 is now running freely
	
	//set up timer capture ccp1/CHRONO_START
	CCP1IE = 0;								//disable interrupt while being configured
#if CHRONO_TRIGGER == RISING
	CCP1CON = 0x05;							//0b0101->rising edge
#else
	CCP1CON = 0x04;							//reset ccp1 to falling edge / default
#endif
	C1TSEL1=C1TSEL0=0;						//0b00->TIMER1 is the time base for CCP1
	CCP1IF = 0;								//clear the flag
	CCP1IE = 1;								//enable ccp1 interrupt

	//set up timer capture ccp2/CHRONO_STOP
	CCP2IE = 0;								//disable interrupt while being configured
#if CHRONO_TRIGGER == RISING
	CCP2CON = 0x05;							//0b0101->rising edge
#else
	CCP2CON = 0x04;							//reset ccp1 to falling edge / default
#endif
	C2TSEL1=C2TSEL0=0;						//0b00->TIMER1 is the time base for CCP2
	CCP2IF = 0;								//clear the flag
	CCP2IE = 1;								//enable ccp2 interrupt
	
	//IOCIF = 0;								//clear the flag
	//IOCIE = 1;								//enable the isr

	PEIE = 1;									//peripheral interrupt on
}

int main(void) {
	uint16_t cnt=0,tmp;							//4-digit display variable
	uint16_t tmr1_prev=0, tmr1_sec=0;;
	
	mcu_init();							   		 //initialize the mcu, 16Mhz
	
	//set up led display
	led_init();									//reset the led
	chrono_init();								//reset the chrono
	
	ei();										//enable global interrupts
	while (1) {
		//if chrono_start/stop are configured as output pins, a transition on them will trigger a capture, per the datasheet
#if 0
		IO_OUT(CHRONO_DDR, CHRONO_START | CHRONO_STOP);
		IO_FLP(LATC, CHRONO_START); IO_FLP(LATC, CHRONO_START);	//strike chrono_start -> start capture should take place here
		//do something here
		delay_us(100/8);						//12.5=700, 25=1220, 50=2220, 100=4220, 200=8220
		IO_FLP(LATC, CHRONO_STOP); IO_FLP(LATC, CHRONO_STOP);	//strike chrono_start -> start capture should take place here
#endif
		//chrono_available should be set, and elapsed time captured in chrono_ticks
		if (chrono_available) {					//if new data is available, display it
			chrono_available = 0;				//reset the flag
			tmp = chrono_ticks/1;					//display chrono_ticks
			//display tmp
			//format lRAM[4]
			lRAM[3]=(tmp % 10) + 0; tmp /= 10;
			lRAM[2]=(tmp % 10) + 0; tmp /= 10;
			lRAM[1]=(tmp % 10) + 0; tmp /= 10;
			lRAM[0]=(tmp % 10) + 0; tmp /= 10;
			//blank leading zero here if you want
			led_display();						//update the display
		}	
		//delay_ms(CHRONO_DLY);					//waste some time
	}
}


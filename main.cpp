/*
 * OrigamiLight.cpp
 *
 * Created: 10/10/2021 16:57:19
 * Author : Windfish
 * Visit windfish.ddns.net
 *
 * Chip used: ATTiny13A
 * The internal oscillator and no prescaling is used for this project.
 * The state of the chip's fuses should be: (E:FF, H:FF, L:7A).
 *
 *								 _________
 * PIN1 - Not connected		   _|	 O    |_		PIN8 - VCC
 * PIN2	- Virtual GND		   _|		  |_		PIN7 - Light sensor
 * PIN3	- Battery sensing	   _|ATTiny13A|_		PIN6 - Tilt/vibration sensor
 * PIN4	- Ground			   _|		  |_		PIN5 - LEDs (PWM)
 *							    |_________|
 */ 


#define F_CPU   8000000
#define BUAD    9600
#define BRC     ((F_CPU/16/BUAD) - 1)

#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#define darknessONtime  15			//This defines how much time the light will stay ON if darkness is detected. Must always be less than the tiltONtime (dedicated to my white wolf)
#define tiltONtime  60				//This defines how much time the light will stay ON if the tilt sensor triggers.

volatile uint8_t secSleep = 100;	//This variable is incremented once every second when the light is ON. When it reaches a threshold, the light goes to sleep

void inline setup()
{
	cli();
	
	DDRB = 0x00;
	PORTB = 0x00;
	DDRB &= ~(1 << PINB1);											//I/O inputs
	PORTB |= 1 << PINB1;											//PULL UP RESISTOR for input
	
	TCCR0A |= (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);			//PWM
	TCCR0B |= 1 << CS02;											//PWM
	OCR0A = 0x00;
	TIMSK0 |= 1 << TOIE0;											//Timer0 overflow interrupt
	MCUCR |= (1 << SM1) | (1 << SE);								//Sleep mode selection
	PCMSK |= (1 << PCINT1);											//Pin change mask
	
	MCUSR = 0;														//Watchdog settings
	WDTCR = (1<<WDCE)|(1<<WDE);
	WDTCR = (1<<WDTIE) | (1<<WDP3) | (1<<WDP0);
	
	sei();
}

void rampUP()					//Dims the light up
{
	DDRB |= 1 << PINB0;			//Sets PINB0 as output
	while (OCR0A < 0xff)
	{
		OCR0A++;				//Increments PWM
		_delay_ms(16);			//Pauses for 16ms each time for a total of 255 * 16ms = 4080ms or 4.080sec for dimming to full brightness and exitig the loop
	}
}

void rampDOWN()					//Dims the light down
{
	while (OCR0A > 0x00)
	{
		OCR0A--;				//Decrements PWM
		_delay_ms(16);			//Pauses for 16ms each time for a total of 255 * 16ms = 4080ms or 4.080sec for dimming to the lowest brightness and exitig the loop
	}
	DDRB &= ~(1 << PINB0);		//Sets PINB0 as input effectively turning off the light completely
}

inline void sePCI()				//Enable pin change interrupt to look for movement of tilt sensor
{
	GIFR &= ~(1 << PCIF);		//Clears pin change interrupt flag
	GIMSK |= 1 << PCIE;			//Set pin change interrupt enable bit
}

inline void clPCI()				//Disables pin change interrupt
{
	GIMSK &= ~(1 << PCIE);		//Clear pin change interrupt enable bit
}

void sleep()
{
	secSleep = 0;
	sePCI();					//Enable pin change interrupt for awakening by reading tile sensor
	sleep_mode();
}

int main(void)
{
	setup();								//Setting up registers
	sleep();								//Immediately enter sleep - wake up sources WATCHDOG ISR and PIN CHANGE INTERRUPT
    while (1)
    {
		if (secSleep <= tiltONtime) 				//When secSleep is less than 60 light is ramped up
		{
			rampUP();
		}
		else										//When secSleep exceeds 60 sec, the light is deemed down and the chip goes to sleep
		{
			rampDOWN();
			sleep();
		}
		if (secSleep > 5)							//When secSleep is more than 5, pin change interrupt is re-enabled so that if someone picks up the origami, it will be detected by the tilt sensor and the on-time will increase
		{
			sePCI();
		}
    }
}

ISR (TIM0_OVF_vect)							//Timer 0 overflow interrupt used for all the timing needs. The prescalre is set to CLOCK/256. This ISR is called approximately 122 times a second
{
	static uint8_t smallTimer = 0;			//The small timer is incremented 122 times to make up one second
	
	smallTimer++;
	if (smallTimer > 122)					//This if is entered once every second
	{
		smallTimer = 0;
		secSleep++;							//secSleep is incremented once a second when the chip is not sleeping
		//DDRB ^= 1 << PINB0;	//Debugging
	}
}

ISR (WDT_vect)									//WDT interrupt to wake from sleep and check brightness once every 8sec
{
	volatile static uint8_t lightTimes = 10;	//How many times light has been detected
	
	WDTCR |= (1<<WDTIE);						//The watchdog timer interrupt enable bit should be written to 1 every time the watchdog ISR executes. If a watchdog timer overflow occurs and this bit is not set, the chip will reset
	//DDRB ^= 1 << PINB0;	//Debugging
	
	if (!OCR0A) secSleep = 100;					//If the light is off, the secSleep is assigned a value greater than the threshold (in this case the threshold is chosen to be 60 in main)
	else return;								//If the light is on, no commands are executed and the routine returns
	
	if (PINB & (1 << PINB2))					//If there photoresistor detects light
	{
		if (lightTimes < 10) lightTimes++;		//The lightTimes is incremented until it reaches 10
	}
	else if (lightTimes >= 10)					//If there photoresistor detects light and there have already been 10 instances of light
	{
		lightTimes = 0;							//The lightTimes is set to 0 so that the light will not keep turning when in the dark
		secSleep = tiltONtime - darknessONtime;	//secSleep is set so that the light will sleep after 15"
		sePCI();								//Pin change interrupt is activated so that if someone picks up the origami, it will be detected by the tilt sensor and the on-time will increase
	}
}

ISR (PCINT0_vect)								//Pin change interrupt used to read the tilt sensor, wake from sleep and extend ON time
{
	clPCI();									//When the pin change ISR is called, it disables itself with this command. It is then re-enabled in various locations in the code
	secSleep = 0;								//Every time the tilt sensor is triggered, the ON time is extended to the maximum (60" chosen as default)
}

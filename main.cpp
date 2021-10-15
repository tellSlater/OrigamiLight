/*
 * OrigamiLight.cpp
 *
 * Created: 10/10/2021 16:57:19
 * Author : tellSlater
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

volatile uint16_t secSleep = 0;

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
	//MCUCR |= (1 << ISC00);											//INT0 interrupt type
	
	MCUSR = 0;														//Watchdog settings
	WDTCR = (1<<WDCE)|(1<<WDE);
	WDTCR = (1<<WDTIE) | (1<<WDP3) | (1<<WDP0);
	
	sei();
}

void sleep()
{
	secSleep = 0;
	GIMSK |= 1 << INT0;			//Enable external interrupt for awakening
	sleep_mode();
}

void rampUP()
{
	DDRB |= 1 << PINB0;
	while (OCR0A < 0xff)
	{
		OCR0A++;
		_delay_ms(16);
	}
}

void rampDOWN()
{
	while (OCR0A > 0x00)
	{
		OCR0A--;
		_delay_ms(16);
	}
	DDRB &= ~(1 << PINB0);
	sleep();
}

inline void seINT0()
{
	GIFR = 0;
	GIMSK |= 1 << INT0;			//Enable external interrupt INT0 to look for movement of tilt sensor
}

inline void clINT0()
{
	GIMSK &= ~(1 << INT0);		//Disable external interrupt INT0
}

int main(void)
{
	setup();
	sei();
	sleep();
    while (1)
    {
		if (secSleep <= 60) rampUP();
		if (secSleep > 5)
		{
			seINT0();
		}
		if (secSleep > 60)
		{
			rampDOWN();
			sleep();
		}
    }
}

ISR (TIM0_OVF_vect) //Timer 0 overflow interrupt used for all the timing needs
{
	static uint8_t smallTimer = 0; 
	smallTimer++;
	if (smallTimer > 122)	//This if is entered once every second
	{
		smallTimer = 0;
		secSleep++;
		//DDRB ^= 1 << PINB0; //Debugging
	}
}

ISR (WDT_vect) //WDT interrupt to wake from sleep and check brightness once every 8sec
{
	volatile static uint8_t lightTimes = 4; //How many times light has been detected
	
	WDTCR |= (1<<WDTIE);
	
	if (!OCR0A) secSleep = 100;
	else return;
	
	if (PINB & (1 << PINB2))
	{
		if (lightTimes < 4) lightTimes++;
	}
	else if (lightTimes >= 4)
	{
		lightTimes = 0;
		secSleep = 45;
		seINT0();
	}
}

ISR (INT0_vect) //External interrupt used to wake from sleep
{
	clINT0();
	secSleep = 0;
}

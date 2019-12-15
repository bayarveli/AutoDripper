/*
 * @file main.c
 *
 *  Created on: 8 �ub 2019
 *      Author: VB
 */

#define F_CPU 16000000UL
#define BAUD 9600UL

#include <avr/io.h>
#include <util/delay.h>
#include <util/setbaud.h>
#include <avr/interrupt.h>
#include <string.h>

typedef enum
{
	WRITE = 0,
	READ = 1
}UsartState;


typedef enum
{
	INIT_COMM = 0,
	UNLOCK_SIM = 1,
	CONFIG_NETWORK = 2,
	OPEN_SOCKET_FOR_LISTEN = 3,
	WAIT_FOR_CONNECTION = 4,
	DATA_TRANSFER = 5,
	NETWORK_OK = 6,
	ACTIVATE = 7
}DripperState;

typedef enum
{
	TRANSACTION_OK = 0,
	TRANSACTION_ERROR = 1
}OperErr;

volatile char atCommand[] = "AT\n";

volatile char txBuffer[64];
volatile unsigned int txIdx;
volatile char rxBuffer[256];
volatile unsigned int rxIndex = 0;
volatile unsigned int startFlag = 0;
volatile unsigned int commandReady = 0;
volatile unsigned int commandExecuted = 0;
volatile unsigned int successFlag = 0;
DripperState dripper = INIT_COMM;


void uart_init(void);
void uart_send(char* c);
void uart_putchar(char c);
unsigned int usart_getch();

void executeCommand(char*, char*, UsartState);


ISR(USART1_RX_vect)
{
	// process data RX Complete

	// Called when data received from USART

	// Read UDR register to reset flag
	unsigned char data = UDR1;
	//
	// Check for error
	if((UCSR1A & ((1 << FE1) | (1 << DOR1) | (1 << UPE1))) == 0)
	{
		rxBuffer[rxIndex++] = data;
		rxBuffer[rxIndex] = '\0';
		if(rxIndex > 256)
		{
			rxIndex = 0;
		}
//		if(rxBuffer[rxIndex] == '\0')
//		{
//			rxBuffer[rxIndex] = '\0';
//			commandReady = 1;
//			rxIndex = 0;
//		}
		commandReady = 1;
	}
}

ISR(USART1_TX_vect)
{
	// switch to listen mode

}

ISR(USART1_UDRE_vect)
{

}

/**
 * ... text ... mainFunction
 */
int main(void)
{

	OperErr status = TRANSACTION_OK;

	DDRB |= 1 << PB5; /* set PB0 to output */
	DDRB |= 1 << PB4; /* set PB4 to output */
	DDRE |= 1 << PE6; /* set PE6 to output */
	// GSM RST
	DDRD |= 1 << PD4; /* set PB4 to output */

	// VALVE
	PORTE |= 1 << PE6;


	PORTD |= 1 << PD4;
	_delay_ms(500);
	PORTD &= ~(1<<PD4);


	_delay_ms(10000);

	PORTB &= ~(1<<PB4); /* LED off */
	PORTB &= ~(1<<PB5);
	// PORTB |= 1 << PB5;
    uart_init();
    sei();

    while(1)
    {
    	switch(dripper)
    	{
			case INIT_COMM:
				executeCommand("AT", "OK", WRITE);
				_delay_ms(100);
				executeCommand("AT&K0", "OK", WRITE);
				_delay_ms(100);
				executeCommand("AT+IPR=9600", "OK", WRITE);
				_delay_ms(100);
				executeCommand("AT+CMEE=2", "OK", WRITE);
				_delay_ms(100);
				dripper = UNLOCK_SIM;
				break;
			case UNLOCK_SIM:
				executeCommand("AT+CPIN=9124", "OK", WRITE);
				_delay_ms(100);
				dripper = NETWORK_OK;
				break;
			case NETWORK_OK:
				executeCommand("AT+CREG?", "+CREG:", WRITE);
				if(successFlag == 1)
				{
					// PORTB |= 1 << PB4;
					dripper = CONFIG_NETWORK;
					_delay_ms(1000);
					// PORTB &= ~(1<<PB4);
					// PORTB &= ~(1<<PB5);
					successFlag = 0;
				}
				// _delay_ms(200000);
				// dripper = CONFIG_NETWORK;
				break;
			case CONFIG_NETWORK:
				executeCommand("AT#FRWL=1,\"0.0.0.0\",\"0.0.0.0\"", "OK", WRITE);
				_delay_ms(100);
				// PORTB |= 1 << PB4;
				executeCommand("AT+CGDCONT=1,\"IP\"\"mgbs\",\"0.0.0.0\",0,0", "OK", WRITE);
				_delay_ms(100);
				// PORTB |= 1 << PB5;
				dripper = ACTIVATE;
				break;
			case ACTIVATE:
				executeCommand("AT#SGACT=1,1", "#SGACT: 188.59.141.92", WRITE);
				// PORTB &= ~(1<<PB4);
				dripper = OPEN_SOCKET_FOR_LISTEN;
				_delay_ms(1000);
				break;
			case OPEN_SOCKET_FOR_LISTEN:
				executeCommand("AT#SKTL=1,0,13005,0", "OK", WRITE);
				PORTB |= 1 << PB4;
				dripper = WAIT_FOR_CONNECTION;
				break;
			case WAIT_FOR_CONNECTION:
				executeCommand("","CONNECT", READ);
				dripper = DATA_TRANSFER;
				PORTB &= ~(1<<PB4);
				// PORTB &= ~(1<<PB5);
				PORTE &= ~(1<<PE6);

				_delay_ms(1000);
				break;
			case DATA_TRANSFER:
				executeCommand("", "#VALF", READ);
				// PORTB |= 1 << PB5;
				break;
			default:
				break;
    	}

    }
    return 0;
}


void executeCommand(char * command, char* expectedResponse, UsartState initialState)
{
	UsartState state = initialState;
	int i = 0;

	while(commandExecuted != 1)
	{
		switch(state)
		{
			case WRITE:
				uart_send(command);
				state = READ;
				break;
			case READ:
				if(commandReady == 1)
				{
					// search for ok.

					commandReady = 0;
					if(strstr(rxBuffer, expectedResponse) != 0)
					{
						if(strstr(rxBuffer,"0,1"))
						{
							successFlag = 1;
							rxIndex = 0;
						}
						else
						{
							rxIndex = 0;
							successFlag = 0;
						}


						if(strstr(rxBuffer, "_ON*") != 0)
						{
							// Valf open
							PORTE |= 1 << PE6;
							// rxIndex = 0;
						}

						if(strstr(rxBuffer, "_OFF*") != 0)
						{
							// Valf close
							PORTE &= ~(1 << PE6);
							// rxIndex = 0;
						}

						commandExecuted = 1;

					}
					else
					{
						// PORTB |= 1 << PB5;
						successFlag = 0;
					}

					if(strstr(rxBuffer, "NO CARRIER") != 0)
					{
						PORTB |= 1 << PB5;
						_delay_ms(2000);
						PORTB &= ~(1<<PB5);
						dripper = OPEN_SOCKET_FOR_LISTEN;
						rxIndex = 0;
						for(i = 0; i < 256; i++)
						{
							rxBuffer[i] = '\0';
						}

						commandExecuted = 1;
					}

				}
				break;
			default:
				break;
		}
	}

	// reset to initial condition
	commandExecuted = 0;
}

void uart_init(void)
{
    UBRR1H = UBRRH_VALUE;
    UBRR1L = UBRRL_VALUE;

    UCSR1A &= ~(_BV(U2X1));

    UCSR1C = _BV(UCSZ11) | _BV(UCSZ10); /* 8-bit data */
    UCSR1B = _BV(RXEN1) | _BV(TXEN1) | _BV(RXCIE1);   /* Enable RX and TX */
}

void uart_putchar(char c) {
    loop_until_bit_is_set(UCSR1A, UDRE1); /* Wait until data register empty. */
    UDR1 = c;
}

void uart_send(char* c)
{
	unsigned idx = 0;

	while(c[idx] != '\0')
	{
		uart_putchar(c[idx]);
		idx++;
	}

	uart_putchar(0x0D);
	uart_putchar(0x0A);
}

unsigned int usart_getch()
{
	while ((UCSR1A & (1 << RXC1)) == 0);
	// Do nothing until data has been received and is ready to be read from UDR
	return (UDR1); // return the byte
}



//switch(state)
//{
//	case WRITE:
//		strcpy((char *)txBuffer, "AT");
//		uart_send(txBuffer);
//		state = READ;
//		break;
//	case READ:
//
//		if(commandReady == 1)
//		{
//			strcpy((char *)txBuffer, (char *)rxBuffer);
//			uart_send(txBuffer);
//			commandReady = 0;
//		}
//		break;
//	default:
//		break;
//}



//DDRB |= 1 << PB5; /* set PB0 to output */
//DDRB |= 1 << PB4; /* set PB4 to output */
//
//PORTB &= ~(1<<PB4); /* LED on */
//PORTB &= ~(1<<PB5); /* LED on */
//    PORTB &= ~(1<<PB4); /* LED on */
//    _delay_ms(900);
//    PORTB |= 1<<PB4; /* LED off */
//    _delay_ms(900);
//    PORTB &= ~(1<<PB5); /* LED on */
//    _delay_ms(900);
//    PORTB |= 1<<PB5; /* LED off */
//    _delay_ms(900);

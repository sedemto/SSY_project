/*
 * uart.c
 *
 * Created: 3/12/2024 10:42:53
 *  Author: Student
 */ 

#include "uart.h"

void UART_init(uint16_t Baudrate){
	uint8_t ubrr = F_CPU/16/Baudrate - 1;
	UBRR1H = (uint8_t)(ubrr>>8);
	UBRR1L = (uint8_t)ubrr;
	
	UCSR1B = (1<<RXEN1)|(1<<TXEN1);
	UCSR1C = (1<<UCSZ01)|(1<<UCSZ00);
	
	UCSR1B |= (1<<RXCIE1);
	
}
void UART_SendChar(uint8_t data){
	while(!tbi(UCSR1A,UDRE1));
	UDR1 = data;
}
void UART_SendString(char *text){
	while(*text!=0x00){
		UART_SendChar(*text);
		text++;
	}
}

uint8_t UART_GetChar(void){
	while (!tbi(UCSR1A,RXC1));
	return UDR1;
}
int printCHAR(char character, FILE *stream)
{
	while ((UCSR1A & (1 << UDRE1)) == 0) {};

	UDR1 = character;

	return 0;
}
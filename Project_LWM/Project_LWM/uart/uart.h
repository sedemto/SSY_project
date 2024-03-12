/*
 * uart.h
 *
 * Created: 3/12/2024 10:43:03
 *  Author: Student
 */ 


#ifndef UART_H_
#define UART_H_


#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "../makra.h"


void UART_init(uint16_t Baudrate);
void UART_SendChar(uint8_t data);
void UART_SendString(char *text);
uint8_t UART_GetChar(void);
int printCHAR(char character, FILE *stream);

#endif /* UART_H_ */
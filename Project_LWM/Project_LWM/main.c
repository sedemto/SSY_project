/*
 * Project_LWM.c
 *
 * Created: 3/12/2024 10:30:31
 * Author : Student
 */ 

#include <avr/io.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

FILE uart_str = FDEV_SETUP_STREAM(printCHAR, NULL, _FDEV_SETUP_RW);
int main(void)
{
	stdout = &uart_str;
    /* Replace with your application code */
    while (1) 
    {
    }
}


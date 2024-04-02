/*
 * makra.h
 *
 * Created: 3/12/2024 10:45:52
 *  Author: Student
 */ 

#include <avr/io.h>

#ifndef MAKRA_H_
#define MAKRA_H_




//Bitove operace
#define sbi_(var, mask)  ((var) |= (uint8_t)(1 << mask))
#define cbi_(var, mask)  ((var) &= (uint8_t)~(1 << mask))
#define tbi_(var,mask)	(var & (1 << mask) )
#define xbi_(var,mask)	((var)^=(uint8_t)(1 << mask))
#define sbiX_(var, mask) (var|= (1 << mask))

//hardware
#define LED0_PIN B,4
#define LED1_PIN B,5

#define LED0ON cbi_(PORTB,4)
#define LED0OFF sbi_(PORTB,4)
#define LED0CHANGE xbi_(PORTB,4)
#define LED1ON cbi_(PORTB,5)
#define LED1OFF sbi_(PORTB,5)
#define LED1CHANGE xbi_(PORTB,5)
#define LED2ON cbi_(PORTB,6)
#define LED2OFF sbi_(PORTB,6)
#define LED2CHANGE xbi_(PORTB,6)
#define LED3ON cbi_(PORTE,3)
#define LED3OFF sbi_(PORTE,3)
#define LED3CHANGE xbi_(PORTE,3)




#endif /* MAKRA_H_ */
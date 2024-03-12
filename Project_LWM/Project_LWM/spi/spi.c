/*
 * spi.c
 *
 * Created: 3/12/2024 10:47:54
 *  Author: Student
 */ 
#include <avr/io.h>
#include "spi.h"
#include "../makra.h"
/*
 * Initialize SPI bus.
 */


void
spi_init(void)
{
  // CS PIN for FLASH
  DDRD	|= BV(WIZNET_CS); // CS to OUT && Disable
  SPI_WIZNET_DISABLE();
  
  /* Initalize ports for communication with SPI units. */
  /* CSN=SS and must be output when master! */
  DDRB  |= BV(MOSI) | BV(SCK);
  PORTB |= BV(MOSI) | BV(SCK);
  DDRD |= BV(CSN);
 
  /* Enables SPI, selects "master", clock rate FCK / 4 - 4Mhz, and SPI mode 0 */
  SPCR = BV(SPE) | BV(MSTR);
#if defined(SPI_8_MHZ)
  SPSR = BV(SPI2X); //FCK / 2 - 8Mhz
#elif defined (SPI_4_MHZ)
  SPSR = 0x0; //FCK / 4 - 4Mhz
#else
  SPSR = 0x0; //FCK / 4 - 4Mhz
#endif


}
/*
 * Project_LWM.c
 *
 * Created: 3/12/2024 10:30:31
 * Author : Student
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <compat/deprecated.h>  //sbi, cbi etc..
#include "avr/wdt.h" // WatchDog
#include <stdio.h>  // printf etc..
#include "uart/uart.h"
#include "spi/spi.h"
#include "makra.h"
#include "Ethernet/socket.h"
#include "Ethernet/wizchip_conf.h"
#include "Application/loopback/loopback.h"
#include "Application/PING/ping.h"
#include "lwm/lwm.h"

// test
// #include "hal.h"
// #include "phy.h"
// #include "sys.h"
// #include "nwk.h"
// #include "sysTimer.h"
// #include "halBoard.h"
// #include "halUart.h"
// #include "config.h"

//#include <stdlib.h> // itoa etc..
/*
 *
 * (8) ICMP PING Client-Server unblocking via IPRAW mode
 *
 * (3) Trying WIZNET5500 init with using official Wiznet ioLibrary_Driver
 * working ping on static IP
 * LED1 = ON when phy_link detected
 * and loopback test on TCP-IP:5000 and UDP:3000 ports.
 * use Hercules terminal utility to check network connection see:
 *
 * https://wizwiki.net/wiki/doku.php?id=osh:cookie:loopback_test
 * https://www.hw-group.com/software/hercules-setup-utility
 *
 */


#define PRINTF_EN 1
#if PRINTF_EN
#define PRINTF(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else
#define PRINTF(...)
#endif

/*
 * m1284p minimum template, with one button & one led
 */

//M644P/M1284p Users LEDS:
//LED1/PORTC.4- m644p/m1284p maxxir
//#define led1_conf()      DDRC |= (1<<DDC4)
//#define led1_high()      PORTC |= (1<<PORTC4)
//#define led1_low()       PORTC &= ~(1<<PORTC4)
//#define led1_tgl()     PORTC ^= (1<<PORTC4)
//#define led1_read()     (PORTC & (1<<PORTC4))
//
//#define sw1_conf()      {DDRC &= ~(1<<DDC5); PORTC |= (1<<PORTC5);}
//#define sw1_read()     (PINC & (1<<PINC5))

#ifdef IP_WORK
uint8_t ping_ip[4] = { 192, 168, 53, 109 }; //Ping IP address
//NIC metrics for WORK PC
wiz_NetInfo netInfo = { .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
		.ip   = {192, 168, 53, 199},         // IP address
		.sn   = {255, 255, 255, 0},         // Subnet mask
		.dns =  {8,8,8,8},			  // DNS address (google dns)
		.gw   = {192, 168, 53, 1}, // Gateway address
		.dhcp = NETINFO_STATIC};    //Static IP configuration
#else
uint8_t ping_ip[4] = { 192, 168, 53, 109 }; //Ping IP address
//NIC metrics for another PC (second IP configuration)
wiz_NetInfo netInfo = { .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
		.ip   = {192, 168, 53, 199},         // IP address
		.sn   = {255, 255, 255, 0},         // Subnet mask
		.dns =  {8,8,8,8},			  // DNS address (google dns)
		.gw   = {192, 168, 53, 1}, // Gateway address
		.dhcp = NETINFO_STATIC};    //Static IP configuration
#endif

//***********Prologue for fast WDT disable & and save reason of reset/power-up: BEGIN
uint8_t mcucsr_mirror __attribute__ ((section (".noinit")));

// This is for fast WDT disable & and save reason of reset/power-up
void get_mcusr(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));
void get_mcusr(void)
{
  mcucsr_mirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}
//***********Prologue for fast WDT disable & and save reason of reset/power-up: END

//*********Global vars
#define TICK_PER_SEC 1000UL
volatile unsigned long _millis; // for millis tick !! Overflow every ~49.7 days




//FUNC headers
static void avr_init(void);
void timer0_init(void);
static inline unsigned long millis(void);

//Wiznet FUNC headers
void print_network_information(void);

// RAM Memory usage test
static int freeRam (void)
{
	extern int __heap_start, *__brkval;
	int v;
	int _res = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
	return _res;
}


//******************* MILLIS ENGINE: BEGIN
//ISR (TIMER0_COMP_vect )
ISR (TIMER0_COMPA_vect)
{
	// Compare match Timer0
	// Here every 1ms
	_millis++; // INC millis tick
}

static inline unsigned long millis(void)
{
	unsigned long i;
	cli();
	// Atomic tick reading
	i = _millis;
	sei();
	return i;
}
//******************* MILLIS ENGINE: END

//***************** UART0: BEGIN
// Assign I/O stream to UART
/* define CPU frequency in Mhz here if not defined in Makefile */
//#ifndef F_CPU
//#define F_CPU 16000000UL
//#endif

/* 19200 baud */
//#define UART_BAUD_RATE      19200
#define UART_BAUD_RATE      38400


FILE uart_str = FDEV_SETUP_STREAM(printCHAR, NULL, _FDEV_SETUP_RW);



//***************** WIZCHIP INIT: BEGIN
//#define ETH_MAX_BUF_SIZE	2048
#define ETH_MAX_BUF_SIZE	256

unsigned char ethBuf0[ETH_MAX_BUF_SIZE];
unsigned char ethBuf1[ETH_MAX_BUF_SIZE];

void cs_sel() {
	SPI_WIZNET_ENABLE();
}

void cs_desel() {
	SPI_WIZNET_DISABLE();
}

uint8_t spi_rb(void) {
	uint8_t rbuf;
	SPI_READ(rbuf);
	return rbuf;
}

void spi_wb(uint8_t b) {
	SPI_WRITE(b);
}

void spi_rb_burst(uint8_t *buf, uint16_t len) {
	for (uint16_t var = 0; var < len; var++) {
		SPI_READ(*buf++);
	}
}

void spi_wb_burst(uint8_t *buf, uint16_t len) {
	for (uint16_t var = 0; var < len; var++) {
		SPI_WRITE(*buf++);
	}
}

void IO_LIBRARY_Init(void) {
	uint8_t bufSize[] = {2, 2, 2, 2, 2, 2, 2, 2};

	reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
	reg_wizchip_spi_cbfunc(spi_rb, spi_wb);
	reg_wizchip_spiburst_cbfunc(spi_rb_burst, spi_wb_burst);

	wizchip_init(bufSize, bufSize);
	wizchip_setnetinfo(&netInfo);
	//wizchip_setinterruptmask(IK_SOCK_0);
}
//***************** WIZCHIP INIT: END


//ICMP callback (fire on ICMP request/reply from ping_srv)
/*
 * socket - socket number
 * ip_query - IP from which ICMP query (like 192.168.0.x)
 * type_query - ICMP query type: PING_REQUEST or PING_REPLY
 * id_query - ICMP query Identificator: ID ICMP [0..0xFFFF]
 * seq_query - ICMP query Sequence Number : ID Seq num [0..0xFFFF]
 * len_query - ICMP query length of the data
 */
void icmp_cb(uint8_t socket,\
		uint8_t* ip_query,\
		uint8_t type_query,\
		uint16_t id_query,\
		uint16_t seq_query,\
		uint16_t len_query)
{
	PRINTF( "<< PING %s from %d.%d.%d.%d ID:%x Seq:%x data:%u bytes\r\n",\
			type_query? "Request": "Reply",\
			(int16_t) ip_query[0],\
			(int16_t) ip_query[1],\
			(int16_t) ip_query[2],\
			(int16_t) ip_query[3],\
			id_query,\
			seq_query,\
			len_query);
}

int main()
{
	//uint8_t prev_sw1 = 1; // VAR for sw1 pressing detect
	SYS_Init();
	// INIT MCU
	avr_init();
	spi_init(); //SPI Master, MODE0, 4Mhz(DIV4), CS_PB.3=HIGH - suitable for WIZNET 5x00(1/2/5)


	// Print program metrics
	//PRINTF("%S", str_prog_name);// ???????? ?????????
	//PRINTF("Compiled at: %S %S\r\n", compile_time, compile_date);// ????? ???? ??????????
	//PRINTF(">> MCU is: %S; CLK is: %luHz\r\n", str_mcu, F_CPU);// MCU Name && FREQ
	PRINTF(">> Free RAM is: %d bytes\r\n", freeRam());

	//Short Blink LED 3 times on startup
	unsigned char i = 3;
	while(i--)
	{
		LED0ON;
		_delay_ms(100);
		LED0OFF;
		_delay_ms(400);
		wdt_reset();
	}

	//Wizchip WIZ5500 Ethernet initialize
	IO_LIBRARY_Init(); //After that ping must working
	print_network_information();

	/* Loopback Test: TCP Server and UDP */
	// Test for Ethernet data transfer validation
	uint32_t timer_link_1sec = millis();
	uint32_t timer_ping1 = millis();
	uint32_t timer_ping2 = millis();
	while(1)
	{	
// 		appTimer.interval = 1000;
// 		appTimer.mode = SYS_TIMER_PERIODIC_MODE;
// 		appTimer.handler = appTimerHandler;
// 		SYS_TimerStart(&appTimer);
		SYS_TaskHandler();
		HAL_UartTaskHandler();
		APP_TaskHandler();
		//Here at least every 1sec
		wdt_reset(); // WDT reset at least every sec

		//Use Hercules Terminal to check loopback tcp:5000 and udp:3000
		/*
		 * https://www.hw-group.com/software/hercules-setup-utility
		 * */
		loopback_tcps(0,ethBuf0,5000);
		loopback_udps(1, ethBuf0, 3000);

		/*
		 * run ICMP (ping) server
		 */
		ping_srv(2);

		/*ICM Ping client example #1 - ping GW/myPC every 10 sec*/
		if((millis()-timer_ping1)> 10000)
		{
			timer_ping1 = millis();
			//PRINTF("\r\n>> PING GW\r\n");
			//ping_request(2, netInfo.gw);

			PRINTF("\r\n>> PING my PC\r\n");
			ping_request(2, ping_ip); //DEVELOPER PC IP
		}

		/*ICM Ping client example #2 - ping DNS google  every 15 sec*/
		if((millis()-timer_ping2)> 15000)
		{
			timer_ping2 = millis();
			PRINTF("\r\n>>> PING DNS\r\n");
			ping_request(2, netInfo.dns);
		}

		//loopback_ret = loopback_tcpc(SOCK_TCPS, gDATABUF, destip, destport);
		//if(loopback_ret < 0) printf("loopback ret: %ld\r\n", loopback_ret); // TCP Socket Error code

		if((millis()-timer_link_1sec)> 1000)
		{
			//here every 1 sec
			timer_link_1sec = millis();
			if(wizphy_getphylink() == PHY_LINK_ON)
			{
				LED0ON;
			}
			else
			{
				LED0OFF;
			}
		}

	}
	return 0;
}

// Timer0
// 1ms IRQ
// Used for millis() timing
void timer0_init(void)
{
	/*
	 *
	 * For M128
	TCCR0 = (1<<CS02)|(1<<WGM01); //TIMER0 SET-UP: CTC MODE & PS 1:64
	OCR0 = 249; // 1ms reach for clear (16mz:64=>250kHz:250-=>1kHz)
	TIMSK |= 1<<OCIE0;	 //IRQ on TIMER0 output compare
	 */
	//For M664p
	cli();
	TCCR0A = 0; TCCR0B = 0; TIMSK0 =  0;
	
	TCCR0A = (1<<WGM01); //TIMER0 SET-UP: CTC MODE
	TCCR0B = (1<<CS01)|(1<<CS00); // PS 1:64
	OCR0A = 249; // 1ms reach for clear (16mz:64=>250kHz:250-=>1kHz)
	TIMSK0 |= 1<<OCIE0A;	 //IRQ on TIMER0 output compareA
	sei();
}

static void avr_init(void)
{
	// Initialize device here.
	// WatchDog INIT
	wdt_enable(WDTO_8S);  // set up wdt reset interval 2 second
	wdt_reset(); // wdt reset ~ every <2000ms

	timer0_init();// Timer0 millis engine init

	// Initial UART Peripheral
	/*
	 *  Initialize uart11 library, pass baudrate and AVR cpu clock
	 *  with the macro
	 *  uart1_BAUD_SELECT() (normal speed mode )
	 *  or
	 *  uart1_BAUD_SELECT_DOUBLE_SPEED() ( double speed mode)
	 */

	UART_init( UART_BAUD_RATE );
	// Define Output/Input Stream
	stdout = &uart_str;
	sei(); //re-enable global interrupts

	return;
}

void print_network_information(void)
{

	uint8_t tmpstr[6] = {0,};
	ctlwizchip(CW_GET_ID,(void*)tmpstr); // Get WIZCHIP name
    PRINTF("\r\n=======================================\r\n");
    PRINTF(" WIZnet chip:  %s \r\n", tmpstr);
    PRINTF("=======================================\r\n");

	wiz_NetInfo gWIZNETINFO;
	wizchip_getnetinfo(&gWIZNETINFO);
	if (gWIZNETINFO.dhcp == NETINFO_STATIC)
		PRINTF("STATIC IP\r\n");
	else
		PRINTF("DHCP IP\r\n");
	printf("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n\r",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
	printf("IP address : %d.%d.%d.%d\n\r",gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
	printf("SM Mask	   : %d.%d.%d.%d\n\r",gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
	printf("Gate way   : %d.%d.%d.%d\n\r",gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
	printf("DNS Server : %d.%d.%d.%d\n\r",gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
}

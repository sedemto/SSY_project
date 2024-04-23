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
#include "mqtt_interface.h"
#include "MQTTClient.h"
#include "json_decoder.h"
#include <math.h>



// PRINTF redefined
#define PRINTF_EN 1
#if PRINTF_EN
#define PRINTF(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else
#define PRINTF(...)
#endif


// NET INFO
uint8_t mqtt_target[4] = {34, 249, 184, 60}; //mqtt IP address
//uint8_t ping_ip[4] = { 192, 168, 53, 109 };	
//NIC metrics for another PC (second IP configuration)

wiz_NetInfo netInfo = { .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
.ip   = {192, 168, 53, 199},         // IP address
.sn   = {255, 255, 255, 0},         // Subnet mask
.dns =  {8,8,8,8},			  // DNS address (google dns)
.gw   = {192, 168, 53, 1}, // Gateway address
.dhcp = NETINFO_STATIC};    //Static IP configuration


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
uint16_t buffer_data;
uint8_t buffer_mask;
uint16_t clusterID;
uint8_t data_ready = 0;
char json_buffer[100] = {0};
	
typedef struct json
{
	char final_json[100];
	char topic[50];
	int len_json;

}json_info;
uint8_t json_config_ready = 0;
uint16_t mqtt_timer = 5000;
int vypis_cau = 0;
json_info info;

//******************* MQTT: BEGIN
#define SOCK_MQTT       5
// Receive Buffer
#define MQTT_BUFFER_SIZE	512     // 2048
uint8_t mqtt_readBuffer[MQTT_BUFFER_SIZE];
volatile uint16_t mes_id;

#define PUBLISH_CONFIG_0         "/ssy/test/config"
#define PUBLISH_TEPLOTA_0         "/ssy/test/teplota"
//#define PUBLISH_AVR_DEBUG         "/w5500_avr_dbg"
//MQTT subscribe call-back is here
void messageArrived(MessageData* md)
{
	char _topic_name[64] = "\0";
	char _message[128] = "\0";

	MQTTMessage* message = md->message;
	
	MQTTString* topic = md->topicName;
	strncpy(_topic_name, topic->lenstring.data, topic->lenstring.len);
	strncpy(_message, message->payload, message->payloadlen);
	
	if(!strcmp(_topic_name,"/ssy/test/config")){
		json_config_ready = 1;
		strncpy(json_buffer, message->payload, message->payloadlen);

	}
	PRINTF("<<MQTT Sub: [%s] %s", _topic_name , _message);
}

void mqtt_pub(Client* mqtt_client, char * mqtt_topic, char * mqtt_msg, int mqtt_msg_len)
{
	static uint32_t mqtt_pub_count = 0;
	static uint8_t mqtt_err_cnt = 0;
	int32_t mqtt_rc;

	wdt_reset();
	//wdt_disable();
	PRINTF(">>MQTT pub msg %lu ", ++mqtt_pub_count);
	MQTTMessage pubMessage;
	pubMessage.qos = QOS0;
	pubMessage.id = mes_id++;
	pubMessage.payloadlen = (size_t)mqtt_msg_len;
	pubMessage.payload = mqtt_msg;
	
	
	mqtt_rc = MQTTPublish(mqtt_client, mqtt_topic , &pubMessage);
	//Analize MQTT publish result (for MQTT failover mode)
	if (mqtt_rc == SUCCESSS)
	{
		mqtt_err_cnt  = 0;
		PRINTF(" - OK\r\n");
	}
	else
	{
		PRINTF(" - ERROR\r\n");
		//Reboot device after 20 continuous errors (~ 20sec)
		if(mqtt_err_cnt++ > 20)
		{
			PRINTF("Connection with MQTT Broker was lost!!\r\nReboot the board..\r\n");
			while(1);
		}
	}
}
//******************* MQTT: END

//FUNC headers
static void avr_init(void);
void timer0_init(void);
unsigned long millis(void);
void executeCommand(char *command);
static void create_json();
int index_mask(int mask);
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
ISR (TIMER0_COMPA_vect)
{
	// Compare match Timer0
	// Here every 1ms
	_millis++; // INC millis tick
}

unsigned long millis(void)
{
	unsigned long i;
	cli();
	// Atomic tick reading
	i = _millis;
	sei();
	return i;
}
//******************* MILLIS ENGINE: END

//***************** UART: BEGIN
#define UART_BAUD_RATE      38400
FILE uart_str = FDEV_SETUP_STREAM(printCHAR, NULL, _FDEV_SETUP_RW);
//***************** UART: END


//***************** JSON: BEGIN
// define sensors
char *meteorologicke[50] = {"Teplota", "Tlak", "Vlhkost vzduchu","Osvetlenie","CO2 - kvalita ovzdusi","Slunecni zareni","Prutok vzduchu","Hluk"};
char *analog2[50] = {"Akcelerometer_x", "Akcelerometer_y", "Akcelerometer","Naklon_x","Naklon_y","Naklon_z","Rychlost pohybu","Rezerva"};
char *zdravotni[50] = {"Tep", "Okyslyceni krve", "Tlak H","Tlak L","Teplota","Glukoza","Dechova frekvence","Rezerva"};
char *analog_2[50] = {"PH", "Vyska hladiny", "Prietok kapaliny","Vlhkost pudy","Hustota","Koncentrace chloru","Vibrace","Rezerva"};
char pasive[8][50] = {"Odpor", "Indukcnost", "Kapacita", "PN", "Impedancia", "Rez freq", "Tau", "Faktor kvality"};
char active[8][50] = {"Napatie", "Proud", "Freq", "Vykon", "Strida", "Ucinnik", "Fazovy posun", "Rezerva"};
char analog3[8][50] = {"Vzdialenost", "Mag pole", "Ionizacni zareni", "Koncetrace CO %", "Barva R", "Barva G", "Barva B", "Rezerva"};
char digital1[8][50] = {"Pritomnost osob", "Detekcia pohybu", "Mag kontakt", "Pritomnost alko", "Detekcia ohna", "Detekcia koure", "Priblizenie", "Zaplaveni"};

// set index based on mask
int index_mask(int mask){
	int index = 0;
	switch(mask){
		case 1:
			index = 0;
			break;
		case 2:
			index = 1;
			break;
		case 4:
			index = 2;
			break;
		case 8:
			index = 3;
			break;
		case 16:
			index = 4;
			break;
		case 32:
			index = 5;
			break;
		case 64:
			index = 6;
			break;
		case 128:
			index = 7;
			break;
		
	}
	return index;
}

void executeCommand(char *command)
{
	jsonNode_t *root = 0;
	jsonDecoderStatus_t ret;

	int cislo_timer;
	int cislo_vypis;

	ret = JSON_DECODER_fromString(command);
	if(JSON_DECODER_OK != ret)
	{
		printf("Invalid JSON string.\r\n");
	}

	JSON_DECODER_getRoot(&root);
	// setting variables based on configuration info
	ret = JSON_DECODER_getNumber(root, "mqtt_timer", &cislo_timer);
	if(JSON_DECODER_OK == ret)
	{
		mqtt_timer = cislo_timer;
	}

	ret = JSON_DECODER_getNumber(root, "vypis_cau", &cislo_vypis);
	if(JSON_DECODER_OK == ret)
	{
		vypis_cau = cislo_vypis;
	}
}static void create_json(){	wdt_reset();	info.final_json[100] = "\0";	char* json_string = "\0";	// set sensor based on clusterID and sent data received	switch(clusterID){		case 1:			json_string = meteorologicke[index_mask(buffer_mask)];			break;		case 2:			json_string = analog2[index_mask(buffer_mask)];			break;		case 3:			json_string = zdravotni[index_mask(buffer_mask)];			break;		case 4:			json_string = analog_2[index_mask(buffer_mask)];			break;		case 5:			json_string = analog3[index_mask(buffer_mask)];			break;		case 6:			json_string = active[index_mask(buffer_mask)];			break;		case 7:			json_string = pasive[index_mask(buffer_mask)];			break;		case 129:			json_string = digital1[index_mask(buffer_mask)];			break;// 		case 130:
// 			json_string = meteorologicke[index_mask(buffer_mask)];
// 			break;
// 		case 131:
// 			json_string = meteorologicke[index_mask(buffer_mask)];
// 			break;
		case 0:			json_string = meteorologicke[index_mask(buffer_mask)];			break;	}	// setting topic based on received data	sprintf(info.topic,"/ssy/test/%s",json_string);	// setting info abount json file -- data (final_json) and length (len_json)	info.len_json = sprintf(info.final_json, "{\"%s\":%d}",json_string,buffer_data);		}//***************** JSON: END

	





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

int main()
{
	
	SYS_Init();
	// INIT MCU
	avr_init();
	spi_init(); //SPI Master, MODE0, 4Mhz(DIV4), CS_PB.3=HIGH - suitable for WIZNET 5x00(1/2/5)
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


	//****************MQTT client initialize
	//Find MQTT broker and connect with it
	uint8_t mqtt_buf[100];
	int32_t mqtt_rc = 0;
	Network mqtt_network;
	Client mqtt_client;
	mqtt_network.my_socket = SOCK_MQTT;
	
	PRINTF(">>Trying connect to MQTT broker: %d.%d.%d.%d ..\r\n", mqtt_target[0], mqtt_target[1], mqtt_target[2], mqtt_target[3]);
	NewNetwork(&mqtt_network);
	ConnectNetwork(&mqtt_network, mqtt_target, 1883);
	MQTTClient(&mqtt_client, &mqtt_network, 1000, mqtt_buf, 100, mqtt_readBuffer, MQTT_BUFFER_SIZE);
	
	//Connection to MQTT broker
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.willFlag = 0;
	data.MQTTVersion = 4;//3;
	data.clientID.cstring = (char*)"w5500_avr_client";
	data.username.cstring = (char*)"user1234";
	data.password.cstring = (char*)"\0";
	data.keepAliveInterval = 60;
	data.cleansession = 1;
	mqtt_rc = MQTTConnect(&mqtt_client, &data);
	if (mqtt_rc == SUCCESSS)
	{
		PRINTF("++MQTT Connected SUCCESS: %ld\r\n", mqtt_rc);
	}
	else
	{
		PRINTF("--MQTT Connected ERROR: %ld\r\n", mqtt_rc);
		while(1);//Reboot the board
	}
	
	// Subscribe to all topics
	char SubString[] = "/ssy/test/#";// Subscribe for all that begin from "/ssy/test/"
	mqtt_rc = MQTTSubscribe(&mqtt_client, SubString, QOS0, messageArrived);
	PRINTF("Subscribed (%s) %d\r\n", SubString, mqtt_rc);
	// timers defined
	uint32_t timer_mqtt_pub_1sec = millis();
	uint32_t timer_link_1sec = millis();

	while(1)
	{	
		SYS_TaskHandler();
		HAL_UartTaskHandler();
		APP_TaskHandler();
		// reset watchdog at every cycle
		wdt_reset();
		// json config
		if(json_config_ready){
			executeCommand(json_buffer);
			json_config_ready = 0;
		}
		// mqtt publish when LWM msg sent
  		if(data_ready){
			create_json();
			// publish data received
  			mqtt_pub(&mqtt_client, info.topic,info.final_json,info.len_json );
  			data_ready = 0;
			// print message when "vypis_cau" is 1
			if(vypis_cau){
				PRINTF("CAAAAU\n\r");
			}
  		}
		// receive subs every mqtt_timer period ( MQTT yield )
 		if((millis()-timer_mqtt_pub_1sec)> mqtt_timer)
 		{
			timer_mqtt_pub_1sec = millis();
			wdt_reset();
			// receive subs
			MQTTYield(&mqtt_client, 100);
			
 		}
		// LINK check
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

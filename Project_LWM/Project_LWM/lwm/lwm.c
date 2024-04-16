/*
 * lwm.c
 *
 * Created: 3/19/2024 10:37:49
 *  Author: Student
 */ 
#include "lwm.h"

#ifdef NWK_ENABLE_SECURITY
#define APP_BUFFER_SIZE     (NWK_MAX_PAYLOAD_SIZE - NWK_SECURITY_MIC_SIZE)
#else
#define APP_BUFFER_SIZE     NWK_MAX_PAYLOAD_SIZE
#endif

typedef enum AppState_t
{
	APP_STATE_INITIAL,
	APP_STATE_IDLE,
} AppState_t;

/*- Variables --------------------------------------------------------------*/
static AppState_t appState = APP_STATE_INITIAL;
static SYS_Timer_t appTimer;
static NWK_DataReq_t appDataReq;
static bool appDataReqBusy = false;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
static uint8_t appUartBuffer[APP_BUFFER_SIZE];
static uint8_t appUartBufferPtr = 0;
extern uint16_t buffer_data;
extern uint8_t buffer_mask;
extern uint16_t clusterID;
extern uint8_t data_ready;


typedef struct PACK
{
	uint8_t      commandId;
	uint8_t      nodeType;
	uint64_t     extAddr;
	uint16_t     shortAddr;
	uint32_t     softVersion;
	uint32_t     channelMask;
	uint8_t      workingChannel;
	uint16_t      panId;
	uint16_t    parentShortAddr;
	uint8_t      lqi;
	int8_t       rssi;

	struct PACK
	{
		uint8_t    maska;
		int16_t    data;
	} sensors;

} AppMessage_t;

typedef struct PACK
{
	uint8_t msg_ID;
	uint16_t node_address;
	uint16_t node_ID;
	uint16_t clusterID;
}AppAddress_t;

void sendOK(int16_t odesilatel){
	if (appDataReqBusy)
	return;
	static char text[]="OK\n\r";
	
	memcpy(appDataReqBuffer, text,sizeof(appDataReqBuffer)-1);
	

	appDataReq.dstAddr = odesilatel;
	appDataReq.dstEndpoint = APP_ENDPOINT_ACK;
	appDataReq.srcEndpoint = APP_ENDPOINT_ACK;
	appDataReq.options = NWK_OPT_ENABLE_SECURITY;
	appDataReq.data = appDataReqBuffer;
	appDataReq.size = sizeof(text)-1;
	appDataReq.confirm = appDataConf;
	NWK_DataReq(&appDataReq);

	//appUartBufferPtr = 0;
	appDataReqBusy = true;
}
void appDataConf(NWK_DataReq_t *req){
		
		appDataReqBusy = false;
		(void)req;
}
void HAL_UartBytesReceived(uint16_t bytes){
	for (uint16_t i = 0; i < bytes; i++)
	{
		uint8_t byte = HAL_UartReadByte();

		if (appUartBufferPtr == sizeof(appUartBuffer)){
			//appSendData();
			sendOK(1); // zadat adresu
		}
		if (appUartBufferPtr < sizeof(appUartBuffer))
		appUartBuffer[appUartBufferPtr++] = byte;
	}

	SYS_TimerStop(&appTimer);
	SYS_TimerStart(&appTimer);
}
void appTimerHandler(SYS_Timer_t *timer){

	(void)timer;
}
bool appDataInd(NWK_DataInd_t *ind){
	AppMessage_t *msg = (AppMessage_t *)ind->data;
	//not whole message, but payload only

	msg->lqi = ind->lqi;
	msg->rssi = ind->rssi;
	buffer_data = msg->sensors.data;
	buffer_mask = msg->sensors.maska;

	sendOK(ind->srcAddr);
	data_ready = 1;
	return true;
}
bool appDataInd_ACK(NWK_DataInd_t *ind)
{
	for (uint8_t i = 0; i < ind->size; i++)
		HAL_UartWriteByte(ind->data[i]);
	
	return true;
}

static bool appAddrInd(NWK_DataInd_t *ind)
{
	printf("Address message \n\r");
	AppAddress_t *addr_msg = (AppAddress_t *)ind->data;
	clusterID = addr_msg->clusterID>>8;
}
void appInit(void){
	NWK_SetAddr(APP_ADDR);
	NWK_SetPanId(APP_PANID);
	PHY_SetChannel(APP_CHANNEL);
	#ifdef PHY_AT86RF212
	PHY_SetBand(APP_BAND);
	PHY_SetModulation(APP_MODULATION);
	#endif
	PHY_SetRxState(true);

	NWK_OpenEndpoint(APP_ENDPOINT, appDataInd);
	NWK_OpenEndpoint(APP_ENDPOINT_ACK, appAddrInd);

	HAL_BoardInit();

	appTimer.interval = APP_FLUSH_TIMER_INTERVAL;
	appTimer.mode = SYS_TIMER_INTERVAL_MODE;
	appTimer.handler = appTimerHandler;
}
void APP_TaskHandler(void){
	switch(appState){
	case APP_STATE_INITIAL:
	{
		appInit();
		appState = APP_STATE_IDLE;
	} break;

	case APP_STATE_IDLE:
	break;

	default:
	break;
}
}
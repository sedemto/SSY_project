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
extern uint8_t buffer[30];
extern uint8_t data_ready;

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
	//char* temp;
	for (uint8_t i = 0; i < ind->size; i++){
		//printf("%c",ind->data[i]);
		buffer[i] = ind->data[i];
		
	}
	//printf(temp);
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
	NWK_OpenEndpoint(APP_ENDPOINT_ACK, appDataInd_ACK);

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
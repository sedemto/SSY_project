/*
 * lwm.h
 *
 * Created: 3/19/2024 10:38:21
 *  Author: Student
 */ 


#ifndef LWM_H_
#define LWM_H_


// includes for LWM
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
// #include "../config.h"
// #include "../stack/hal/atmega256rfr2/inc/hal.h"
// #include "../stack/phy/atmega256rfr2/inc/phy.h"
// #include "../stack/sys/inc/sys.h"
// #include "../stack/nwk/inc/nwk.h"
// #include "../stack/sys/inc/sysTimer.h"
// #include "../stack/hal/drivers/atmega256rfr2/inc/halBoard.h"
// #include "../stack/hal/drivers/atmega256rfr2/inc/halUart.h"
#include "config.h"
#include "hal.h"
#include "phy.h"
#include "sys.h"
#include "nwk.h"
#include "sysTimer.h"
#include "halBoard.h"
#include "halUart.h"
#include "makra.h"


void sendOK(int16_t odesilatel);
void appDataConf(NWK_DataReq_t *req);
void HAL_UartBytesReceived(uint16_t bytes);
void appTimerHandler(SYS_Timer_t *timer);
bool appDataInd(NWK_DataInd_t *ind);
void appInit(void);
void APP_TaskHandler(void);
bool appDataInd_ACK(NWK_DataInd_t *ind);

#endif /* LWM_H_ */
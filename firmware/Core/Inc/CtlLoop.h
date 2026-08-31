#ifndef __CTLLOOP_H
#define __CTLLOOP_H	 

#include "stm32f3xx_hal.h"
#include "function.h"


void BuckBoostVLoopCtlPID(void);

extern int32_t  VErr0,VErr1,VErr2;
extern int32_t	u0,u1;

//一个开关周期数字量 
#define PERIOD 10240	 
#endif


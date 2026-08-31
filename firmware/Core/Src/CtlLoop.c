/* USER CODE BEGIN Header */
/****************************************************************************************
	* 
	* @file           : CtlLoop.c
  * @brief          : 环路功能函数
  * @version V1.0
  * @date    01-03-2021
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
	
/* USER CODE END Header */
#include "CtlLoop.h"


/****************环路变量定义**********************/
int32_t   VErr0=0,VErr1=0,VErr2=0;//电压误差Q12
int32_t		u0=0,u1=0;//电压环输出量

/*
** ===================================================================
**     Funtion Name : void BuckBoostVLoopCtlPID(void)
**     Description :  BUCK-BOOST模式下环路计算
** 										检测输出参考电压与输入电压的关系，判断环路工作于BUCK模式，BOOST模式还是BUCK-BOOST模式
** 										BUCK模式下，BOOST的开关管工作在固定占空比，控制BUCK的占空比控制输出电压
**										BOOST模式下，BUCK的开关管工作在固定占空比，控制BOOST的占空比控制输出电压
**										BUCK-BOOST模式下，BUCK的开关管工作在固定占空比，控制BOOST的占空比控制输出电压
**     Parameters  :无
**     Returns     :无
** ===================================================================
*/
//环路的参数，具体参照mathcad计算文件《buck输出-恒压-PID型补偿器》
#define BUCKPIDb0	5203		//Q8
#define BUCKPIDb1	-10246	//Q8
#define BUCKPIDb2	5044		//Q8
//环路的参数，具体参照mathcad计算文件《BOOST输出-恒压-PID型补偿器》
#define BOOSTPIDb0	8860		//Q8
#define BOOSTPIDb1	-17445	//Q8
#define BOOSTPIDb2	8588		//Q8
CCMRAM void BuckBoostVLoopCtlPID(void)
{		
	int32_t VoutTemp=0;//输出电压矫正后
	
	//输出电压矫正
	VoutTemp = ((uint32_t )ADC1_RESULT[0]*CAL_VOUT_K>>12)+CAL_VOUT_B;
	//计算电压误差量，当参考电压大于输出电压，占空比增加，输出量增加
	VErr0= CtrValue.Voref  - VoutTemp; 
	//当模式切换时，认为降低占空比，确保模式切换不过冲
	//BBModeChange为模式切换为，不同模式切换时，该位会被置1
	if(DF.BBModeChange)
	{
		u1 = 0;
		DF.BBModeChange =0;
	}
	//判断工作模式，BUCK，BOOST，BUCK-BOOST
	switch(DF.BBFlag)
	{
		case  NA ://初始阶段
		{
			VErr0=0;
			VErr1=0;
			VErr2=0;
			u0 = 0;
			u1 = 0;
			break;
		}
		case  Buck:	//BUCK模式	
		{
			//调用PID环路计算公式（参照PID环路计算文档）
			u0 = u1 + VErr0*BUCKPIDb0 + VErr1*BUCKPIDb1 + VErr2*BUCKPIDb2;	
			//历史数据幅值
			VErr2 = VErr1;
			VErr1 = VErr0;
			u1 = u0;			
			//环路输出赋值，u0右移8位为BUCKPIDb0-2为人为放大Q8倍的数字量
			CtrValue.BuckDuty= u0>>8;
			CtrValue.BoostDuty=MIN_BOOST_DUTY1;//BOOST上管固定占空比93%，下管7%			
			//环路输出最大最小占空比限制
			if(CtrValue.BuckDuty > CtrValue.BUCKMaxDuty)
				CtrValue.BuckDuty = CtrValue.BUCKMaxDuty;	
			if(CtrValue.BuckDuty < MIN_BUKC_DUTY)
				CtrValue.BuckDuty = MIN_BUKC_DUTY;
			break;
		}				
		case  Boost://Boost模式
		{
			//调用PID环路计算公式（参照PID环路计算文档）
			u0 = u1 + VErr0*BOOSTPIDb0 + VErr1*BOOSTPIDb1 + VErr2*BOOSTPIDb2;			
			//历史数据幅值
			VErr2 = VErr1;
			VErr1 = VErr0;
			u1 = u0;			
			//环路输出赋值，u0右移8位为BUCKPIDb0-2为人为放大Q8倍的数字量
			CtrValue.BuckDuty = MAX_BUCK_DUTY;//否则固定占空比93%
			CtrValue.BoostDuty = u0>>8;	
			//环路输出最大最小占空比限制
			if(CtrValue.BoostDuty > CtrValue.BoostMaxDuty)
				CtrValue.BoostDuty = CtrValue.BoostMaxDuty;	
			if(CtrValue.BoostDuty < MIN_BOOST_DUTY)
				CtrValue.BoostDuty = MIN_BOOST_DUTY;
			break;
		}
		case  Mix://Mix模式
		{
			//调用PID环路计算公式（参照PID环路计算文档）
			u0 = u1 + VErr0*BOOSTPIDb0 + VErr1*BOOSTPIDb1 + VErr2*BOOSTPIDb2;			
			//历史数据幅值
			VErr2 = VErr1;
			VErr1 = VErr0;
			u1 = u0;			
			//环路输出赋值，u0右移8位为BUCKPIDb0-2为人为放大Q8倍的数字量
			CtrValue.BuckDuty = MAX_BUCK_DUTY1;//否则固定占空比80%
			CtrValue.BoostDuty = u0>>8;	
			//环路输出最大最小占空比限制
			if(CtrValue.BoostDuty > CtrValue.BoostMaxDuty)
				CtrValue.BoostDuty = CtrValue.BoostMaxDuty;	
			if(CtrValue.BoostDuty < MIN_BOOST_DUTY)
				CtrValue.BoostDuty = MIN_BOOST_DUTY;
			break;
		}	
	}
	
	//PWMENFlag是PWM开启标志位，当该位为0时,buck的占空比为0，无输出;
	if(DF.PWMENFlag==0)
		CtrValue.BuckDuty = MIN_BUKC_DUTY;
	//更新对应寄存器
	HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR = CtrValue.BuckDuty * PERIOD>>12; //BUCK占空比
	HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP3xR = HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR>>1; //ADC触发采样点
	HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = PERIOD - (CtrValue.BoostDuty * PERIOD>>12);//Boost占空比
}


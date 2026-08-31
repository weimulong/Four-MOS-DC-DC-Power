/* USER CODE BEGIN Header */
/****************************************************************************************
	* 
	* @file           : Function.c
  * @brief          : Function 功能函数集合
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

#include "function.h"
#include "CtlLoop.h"

struct  _ADI SADC={2048,2048,0,0,2048,2048,0,0,0,0};
struct  _Ctr_value  CtrValue={0,0,0,MIN_BUKC_DUTY,0,0,0};
struct  _FLAG    DF={0,0,0,0,0,0,0,0};
uint16_t ADC1_RESULT[4]={0,0,0,0};
//软启动状态标志位
SState_M 	STState = SSInit ;
//OLED刷新计数 5mS计数一次
uint16_t OLEDShowCnt=0;
/*
** ===================================================================
**     Funtion Name :   void ADCSample(void)
**     Description :    采样输出电压、输出电流、输入电压、输入电流
**     Parameters  :
**     Returns     :
** ===================================================================
*/
CCMRAM void ADCSample(void)
{
	static uint32_t VinAvgSum=0,IinAvgSum=0,VoutAvgSum=0,IoutAvgSum=0;
	
	//从DMA缓冲器中获取数据 Q15,并对其进行线性矫正，反相情况下寄存器对应的值有变化
	SADC.Vin  = ((uint32_t )ADC1_RESULT[2]*CAL_VIN_K>>12)+CAL_VIN_B;
	SADC.Iin  = ((uint32_t )ADC1_RESULT[3]*CAL_IIN_K>>12)+CAL_IIN_B;
	SADC.Vout  = ((uint32_t )ADC1_RESULT[0]*CAL_VOUT_K>>12)+CAL_VOUT_B;
	SADC.Iout  = ((uint32_t )ADC1_RESULT[1]*CAL_IOUT_K>>12)+CAL_IOUT_B;

	if(SADC.Vin <100 )
		SADC.Vin = 0;	
	if(SADC.Iin >2048 )
		SADC.Iin =2048;
	if(SADC.Vout <100 )
		SADC.Vout = 0;
	if(SADC.Iout >2048 )
		SADC.Iout =2048;
	
	//计算各个采样值的平均值-滑动平均方式
	VinAvgSum = VinAvgSum + SADC.Vin -(VinAvgSum>>2);
	SADC.VinAvg = VinAvgSum>>2;
	IinAvgSum = IinAvgSum + SADC.Iin -(IinAvgSum>>2);
	SADC.IinAvg = IinAvgSum >>2;
	VoutAvgSum = VoutAvgSum + SADC.Vout -(VoutAvgSum>>2);
	SADC.VoutAvg = VoutAvgSum>>2;
	IoutAvgSum = IoutAvgSum + SADC.Iout -(IoutAvgSum>>2);
	SADC.IoutAvg = IoutAvgSum>>2;	
}

/** ===================================================================
**     Funtion Name :void StateM(void)
**     Description :   状态机函数
**     初始化状态
**     等外启动状态
**     启动状态
**     运行状态
**     故障状态
**     Parameters  :
**     Returns     :
** ===================================================================*/
void StateM(void)
{
	//判断状态类型
	switch(DF.SMFlag)
	{
		//初始化状态
		case  Init :StateMInit();
		break;
		//等待状态
		case  Wait :StateMWait();
		break;
		//软启动状态
		case  Rise :StateMRise();
		break;
		//运行状态
		case  Run :StateMRun();
		break;
		//故障状态
		case  Err :StateMErr();
		break;
	}
}
/** ===================================================================
**     Funtion Name :void StateMInit(void)
**     Description :   初始化状态函数，参数初始化
**     Parameters  :
**     Returns     :
** ===================================================================*/
void StateMInit(void)
{
	//相关参数初始化
	ValInit();
	//状态机跳转至等待软启状态
	DF.SMFlag  = Wait;
}

/** ===================================================================
**     Funtion Name :void StateMWait(void)
**     Description :   等待状态机，等待1S后无故障则软启
**     Parameters  :
**     Returns     :
** ===================================================================*/
void StateMWait(void)
{
	//计数器定义
	static uint16_t CntS = 0;
	
	//关PWM
	DF.PWMENFlag=0;
	//计数器累加
	CntS ++;
	//等待1S，无故障情况,切按键按下，启动，则进入启动状态
	if(CntS>200)
	{
		CntS=200;
		HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //开启PWM输出和PWM计时器
		if((DF.ErrFlag==F_NOERR)&&(DF.KeyFlag1==1))
		{
			//计数器清0
			CntS=0;
			//状态标志位跳转至等待状态
			DF.SMFlag  = Rise;
			//软启动子状态跳转至初始化状态
			STState = SSInit;
		}
	}
}
/*
** ===================================================================
**     Funtion Name : void StateMRise(void)
**     Description :软启阶段
**     软启初始化
**     软启等待
**     开始软启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define MAX_SSCNT       20//等待100ms
void StateMRise(void)
{
	//计时器
	static  uint16_t  Cnt = 0;
	//最大占空比限制计数器
	static  uint16_t	BUCKMaxDutyCnt=0,BoostMaxDutyCnt=0;

	//判断软启状态
	switch(STState)
	{
		//初始化状态
		case    SSInit:
		{
			//关闭PWM
			DF.PWMENFlag=0;
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭				
			//软启中将运行限制占空比启动，从最小占空比开始启动
			CtrValue.BUCKMaxDuty  = MIN_BUKC_DUTY;
			CtrValue.BoostMaxDuty = MIN_BOOST_DUTY;
			//环路计算变量初始化
			VErr0=0;
			VErr1=0;
			VErr2=0;
			u0 = 0;
			u1 = 0;
			//跳转至软启等待状态
			STState = SSWait;

			break;
		}
		//等待软启动状态
		case    SSWait:
		{
			//计数器累加
			Cnt++;
			//等待100ms
			if(Cnt> MAX_SSCNT)
			{
				//计数器清0
				Cnt = 0;
				//限制启动占空比
				CtrValue.BuckDuty = MIN_BUKC_DUTY;
				CtrValue.BUCKMaxDuty= MIN_BUKC_DUTY;
				CtrValue.BoostDuty = MIN_BOOST_DUTY;
				CtrValue.BoostMaxDuty = MIN_BOOST_DUTY;
				//环路计算变量初始化
				VErr0=0;
				VErr1=0;
				VErr2=0;
				u0 = 0;
				u1 = 0;
				//输出参考电压从一半开始启动，避免过冲
				CtrValue.Voref  = CtrValue.Voref >>1;
				STState = SSRun;	//跳转至软启状态			
			}
			break;
		}
		//软启动状态
		case    SSRun:
		{
			if(DF.PWMENFlag==0)//正式发波前环路变量清0
			{
				//环路计算变量初始化
				VErr0=0;
				VErr1=0;
				VErr2=0;
				u0 = 0;
				u1 = 0;
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //开启PWM输出和PWM计时器
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //开启PWM输出和PWM计时器		
			}
			//发波标志位置位
			DF.PWMENFlag=1;
			//最大占空比限制逐渐增加
			BUCKMaxDutyCnt++;
			BoostMaxDutyCnt++;
			//最大占空比限制累加
			CtrValue.BUCKMaxDuty = CtrValue.BUCKMaxDuty + BUCKMaxDutyCnt*5;
			CtrValue.BoostMaxDuty = CtrValue.BoostMaxDuty + BoostMaxDutyCnt*5;
			//累加到最大值
			if(CtrValue.BUCKMaxDuty > MAX_BUCK_DUTY)
				CtrValue.BUCKMaxDuty  = MAX_BUCK_DUTY ;
			if(CtrValue.BoostMaxDuty > MAX_BOOST_DUTY)
				CtrValue.BoostMaxDuty  = MAX_BOOST_DUTY ;
			
			if((CtrValue.BUCKMaxDuty==MAX_BUCK_DUTY)&&(CtrValue.BoostMaxDuty==MAX_BOOST_DUTY))			
			{
				//状态机跳转至运行状态
				DF.SMFlag  = Run;
				//软启动子状态跳转至初始化状态
				STState = SSInit;	
			}
			break;
		}
		default:
		break;
	}
}
/*
** ===================================================================
**     Funtion Name :void StateMRun(void)
**     Description :正常运行，主处理函数在中断中运行
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void StateMRun(void)
{

}

/*
** ===================================================================
**     Funtion Name :void StateMErr(void)
**     Description :故障状态
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void StateMErr(void)
{
	//关闭PWM
	DF.PWMENFlag=0;
	HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
	HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭		
	//若故障消除跳转至等待重新软启
	if(DF.ErrFlag==F_NOERR)
			DF.SMFlag  = Wait;
}

/** ===================================================================
**     Funtion Name :void ValInit(void)
**     Description :   相关参数初始化函数
**     Parameters  :
**     Returns     :
** ===================================================================*/
void ValInit(void)
{
	//关闭PWM
	DF.PWMENFlag=0;
	HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
	HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭		
	//清除故障标志位
	DF.ErrFlag=0;
	//初始化电压参考量
	CtrValue.Voref=0;
	//限制占空比
	CtrValue.BuckDuty = MIN_BUKC_DUTY;
	CtrValue.BUCKMaxDuty= MIN_BUKC_DUTY;
	CtrValue.BoostDuty = MIN_BOOST_DUTY;
	CtrValue.BoostMaxDuty = MIN_BOOST_DUTY;
	//环路计算变量初始化
	VErr0=0;
	VErr1=0;
	VErr2=0;
	u0 = 0;
	u1 = 0;
}

/** ===================================================================
**     Funtion Name :void VrefGet(void)
**     Description :   从滑动变阻器值获取输出电压参考值，当电压参考值变化时，缓慢增加
**     Parameters  :
**     Returns     :
** ===================================================================*/
#define MAX_VREF    2921//输出最大参考电压48V  0.5V的余量   48.5V/68V*Q12
#define MIN_VREF    271//最低电压参考值5V   0.5V的余量   4.5V/68V*2^Q12
#define VREF_K      10//递增或递减步长
void VrefGet(void)
{
	//电压参考值中间变量
	int32_t VTemp = 0;	
	//滑动平均求和中间变量
	static int32_t VadjSum = 0;

	//获取ADC采样值
	SADC.Vadj = HAL_ADC_GetValue(&hadc2);
	//对采样值滑动求平均
	VadjSum = VadjSum + SADC.Vadj -(VadjSum>>8);
	SADC.VadjAvg = VadjSum>>8;
	
	//参考电压 = MIN_VREF+滑动变阻器采样值
	VTemp = MIN_VREF + SADC.Vadj;
	
	//缓慢递增或缓慢递减电压参考值
	if( VTemp> ( CtrValue.Voref + VREF_K))
			CtrValue.Voref = CtrValue.Voref + VREF_K;
	else if( VTemp < ( CtrValue.Voref - VREF_K ))
			CtrValue.Voref =CtrValue.Voref - VREF_K;
	else
			CtrValue.Voref = VTemp ;

	//MIX模式下调压限制-MIX模式
	if(CtrValue.Voref >(SADC.VinAvg<<1))//输出限制在输入的2*vin 
		CtrValue.Voref =(SADC.VinAvg<<1);
	if( CtrValue.Voref >MAX_VREF )
		CtrValue.Voref =MAX_VREF;
}
/*
** ===================================================================
**     Funtion Name :void ShortOff(void)
**     Description :短路保护，可以重启10次
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define MAX_SHORT_I     651//短路电流判据
#define MIN_SHORT_V     289//短路电压判据
void ShortOff(void)
{
	static int32_t RSCnt = 0;
	static uint8_t RSNum =0 ;

	//当输出电流大于 *A，且电压小于*V时，可判定为发生短路保护
	if((SADC.Iout< MAX_SHORT_I)&&(SADC.Vout <MIN_SHORT_V))
	{
		//关闭PWM
		DF.PWMENFlag=0;
		HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
		HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭	
		//故障标志位
		setRegBits(DF.ErrFlag,F_SW_SHORT);
		//跳转至故障状态
		DF.SMFlag  =Err;
	}
	//输出短路保护恢复
	//当发生输出短路保护，关机后等待4S后清楚故障信息，进入等待状态等待重启
	if(getRegBits(DF.ErrFlag,F_SW_SHORT))
	{
		//等待故障清楚计数器累加
		RSCnt++;
		//等待2S
		if(RSCnt >400)
		{
			//计数器清零
			RSCnt=0;
			//短路重启只重启10次，10次后不重启
			if(RSNum > 10)
			{
				//确保不清除故障，不重启
				RSNum =11;
				//关闭PWM
				DF.PWMENFlag=0;
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭	
			}
			else
			{
				//短路重启计数器累加
				RSNum++;
				//清除过流保护故障标志位
				clrRegBits(DF.ErrFlag,F_SW_SHORT);
			}
		}
	}
}
/*
** ===================================================================
**     Funtion Name :void SwOCP(void)
**     Description :软件过流保护，可重启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define MAX_OCP_VAL     931//*A过流保护点 
void SwOCP(void)
{
	//过流保护判据保持计数器定义
	static  uint16_t  OCPCnt=0;
	//故障清楚保持计数器定义
	static  uint16_t  RSCnt=0;
	//保留保护重启计数器
	static  uint16_t  RSNum=0;

	//当输出电流大于*A，且保持500ms
	if((SADC.Iout < MAX_OCP_VAL)&&(DF.SMFlag  ==Run))
	{
		//条件保持计时
		OCPCnt++;
		//条件保持50ms，则认为过流发生
		if(OCPCnt > 10)
		{
			//计数器清0
			OCPCnt  = 0;
			//关闭PWM
			DF.PWMENFlag=0;
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭	
			//故障标志位
			setRegBits(DF.ErrFlag,F_SW_IOUT_OCP);
			//跳转至故障状态
			DF.SMFlag  =Err;
		}
	}
	else
		//计数器清0
		OCPCnt  = 0;

	//输出过流后恢复
	//当发生输出软件过流保护，关机后等待4S后清楚故障信息，进入等待状态等待重启
	if(getRegBits(DF.ErrFlag,F_SW_IOUT_OCP))
	{
		//等待故障清楚计数器累加
		RSCnt++;
		//等待2S
		if(RSCnt > 400)
		{
			//计数器清零
			RSCnt=0;
			//过流重启计数器累加
			RSNum++;
			//过流重启只重启10次，10次后不重启（严重故障）
			if(RSNum > 10 )
			{
				//确保不清除故障，不重启
				RSNum =11;
				//关闭PWM
				DF.PWMENFlag=0;
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭		
			}
			else
			{
			 //清除过流保护故障标志位
				clrRegBits(DF.ErrFlag,F_SW_IOUT_OCP);
			}
		}
	}
}

/*
** ===================================================================
**     Funtion Name :void SwOVP(void)
**     Description :软件输出过压保护，不重启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define MAX_VOUT_OVP_VAL    3012//50V过压保护	（50/68）*Q12
void VoutSwOVP(void)
{
	//过压保护判据保持计数器定义
	static  uint16_t  OVPCnt=0;

	//当输出电压大于50V，且保持100ms
	if (SADC.Vout > MAX_VOUT_OVP_VAL)
	{
		//条件保持计时
		OVPCnt++;
		//条件保持10ms
		if(OVPCnt > 2)
		{
			//计时器清零
			OVPCnt=0;
			//关闭PWM
			DF.PWMENFlag=0;
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭		
			//故障标志位
			setRegBits(DF.ErrFlag,F_SW_VOUT_OVP);
			//跳转至故障状态
			DF.SMFlag  =Err;
		}
	}
	else
		OVPCnt = 0;
}

/*
** ===================================================================
**     Funtion Name :void VinSwUVP(void)
**     Description :输入软件欠压保护，低压输入保护,可恢复
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define MIN_UVP_VAL    686//11.4V欠压保护 （11.4/68 ）*Q12
#define MIN_UVP_VAL_RE  795//13.2V欠压保护恢复 （13.2/68）*Q12
void VinSwUVP(void)
{
	//过压保护判据保持计数器定义
	static  uint16_t  UVPCnt=0;
	static  uint16_t	RSCnt=0;

	//当输出电流小于于11.4V，且保持200ms
	if ((SADC.Vin < MIN_UVP_VAL) && (DF.SMFlag != Init ))
	{
		//条件保持计时
		UVPCnt++;
		//条件保持10ms
		if(UVPCnt > 2)
		{
			//计时器清零
			UVPCnt=0;
			RSCnt=0;
			//关闭PWM
			DF.PWMENFlag=0;
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭		
			//故障标志位
			setRegBits(DF.ErrFlag,F_SW_VIN_UVP);
			//跳转至故障状态
			DF.SMFlag  =Err;
		}
	}
	else
		UVPCnt = 0;
	
	//输入欠压保护恢复
	//当发生输入欠压保护，等待输入电压恢复至正常水平后清楚故障标志位，重启
	if(getRegBits(DF.ErrFlag,F_SW_VIN_UVP))
	{
		if(SADC.Vin > MIN_UVP_VAL_RE) 
		{
			//等待故障清楚计数器累加
			RSCnt++;
			//等待1S
			if(RSCnt > 200)
			{
				RSCnt=0;
				UVPCnt=0;
				//清楚故障标志位
				clrRegBits(DF.ErrFlag,F_SW_VIN_UVP);
			}	
		}
		else	
			RSCnt=0;	
	}
	else
		RSCnt=0;
}

/*
** ===================================================================
**     Funtion Name :void VinSwOVP(void)
**     Description :软件输入过压保护，不重启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define MAX_VIN_OVP_VAL    3012//50V过压保护	（50/68）*Q12
void VinSwOVP(void)
{
	//过压保护判据保持计数器定义
	static  uint16_t  OVPCnt=0;

	//当输出电压大于50V，且保持100ms
	if (SADC.Vin > MAX_VIN_OVP_VAL )
	{
		//条件保持计时
		OVPCnt++;
		//条件保持10ms
		if(OVPCnt > 2)
		{
			//计时器清零
			OVPCnt=0;
			//关闭PWM
			DF.PWMENFlag=0;
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭		
			//故障标志位
			setRegBits(DF.ErrFlag,F_SW_VIN_OVP);
			//跳转至故障状态
			DF.SMFlag  =Err;
		}
	}
	else
		OVPCnt = 0;
}

/** ===================================================================
**     Funtion Name :void LEDShow(void)
**     Description :  LED显示函数
**     初始化与等待启动状态，红黄绿全亮
**     启动状态，黄绿亮
**     运行状态，绿灯亮
**     故障状态，红灯亮
**     Parameters  :
**     Returns     :
** ===================================================================*/
//输出状态灯宏定义
 #define SET_LED_G()	HAL_GPIO_WritePin(GPIOB, LED_G_Pin,GPIO_PIN_SET)//绿灯亮
 #define SET_LED_Y()	HAL_GPIO_WritePin(GPIOB, LED_Y_Pin,GPIO_PIN_SET)//绿灯亮
 #define SET_LED_R()	HAL_GPIO_WritePin(GPIOB, LED_R_Pin,GPIO_PIN_SET)//绿灯亮
 #define CLR_LED_G()	HAL_GPIO_WritePin(GPIOB, LED_G_Pin,GPIO_PIN_RESET)//绿灯灭
 #define CLR_LED_Y()	HAL_GPIO_WritePin(GPIOB, LED_Y_Pin,GPIO_PIN_RESET)//黄灯灭
 #define CLR_LED_R()	HAL_GPIO_WritePin(GPIOB, LED_R_Pin,GPIO_PIN_RESET)//红灯灭
void LEDShow(void)
{
	switch(DF.SMFlag)
	{
		//初始化状态，红黄绿全亮
		case  Init :
		{
			SET_LED_G();
			SET_LED_Y();
			SET_LED_R();
			break;
		}
		//等待状态，红黄绿全亮
		case  Wait :
		{
			SET_LED_G();
			SET_LED_Y();
			SET_LED_R();
			break;
		}
		//软启动状态，黄绿亮
		case  Rise :
		{
			SET_LED_G();
			SET_LED_Y();
			CLR_LED_R();
			break;
		}
		//运行状态，绿灯亮
		case  Run :
		{
			SET_LED_G();
			CLR_LED_Y();
			CLR_LED_R();
			break;
		}
		//故障状态，红灯亮
		case  Err :
		{
			CLR_LED_G();
			CLR_LED_Y();
			SET_LED_R();
			break;
		}
	}
}

/** ===================================================================
**     Funtion Name :void KEYFlag(void)
**     Description :两个按键的状态
**		 默认状态KEYFlag为0.按下时Flag变1，再次按下Flag变0，依次循环
**		 当机器正常运行，或者启动过程中，按下按键后，关闭输出，进入待机状态
**     Parameters  :
**     Returns     :
** ===================================================================*/
#define READ_KEY1() HAL_GPIO_ReadPin(GPIOB, KEY_1_Pin)
#define READ_KEY2() HAL_GPIO_ReadPin(GPIOB, KEY_2_Pin)
void KEYFlag(void)
{
	//计时器，按键消抖用
	static uint16_t	KeyDownCnt1=0,KeyDownCnt2=0;
	
	//按键按下
	if(READ_KEY1()==0)
	{
		//计时，按键按下150mS有效
		KeyDownCnt1++;
		if(KeyDownCnt1 > 30)
		{
			KeyDownCnt1 = 0;//计时器清零
			//按键状态有变化
			if(DF.KeyFlag1==0)
				DF.KeyFlag1 = 1;
			else
				DF.KeyFlag1 = 0;
		}
	}
	else
		KeyDownCnt1 = 0;//计时器清零
	
	//按键按下
	if(READ_KEY2()==0)
	{
		//计时，按键按󒑃0mS有效
		KeyDownCnt2++;
		if(KeyDownCnt2 > 30)
		{
			KeyDownCnt2 = 0;//计时器清零
			//按键状态有变化
			if(DF.KeyFlag2==0)
				DF.KeyFlag2 = 1;
			else
				DF.KeyFlag2 = 0;
		}
	}
	else
		KeyDownCnt2 = 0;//计时器清零

	//当机器正常运行，或者启动过程中，按下按键后，关闭输出，进入待机状态
	if((DF.KeyFlag1==0)&&((DF.SMFlag==Rise)||(DF.SMFlag==Run)))
	{
		DF.SMFlag = Wait;
		//关闭PWM
		DF.PWMENFlag=0;
		HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2); //关闭
		HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2); //关闭	
	}
}

/** ===================================================================
**     Funtion Name :void BBMode(void)
**     Description :运行模式判断
** 		 BUCK模式：输出参考电压<0.8倍输入电压
** 		 BOOST模式：输出参考电压>1.2倍输入电压
**		 MIX模式：1.2倍输入电压>输出参考电压>0.8倍输入电压
**		 当进入MIX（buck-boost）模式后，退出到BUCK或者BOOST时需要滞缓，防止在临界点来回振荡
**     Parameters  :
**     Returns     :
** ===================================================================*/
void BBMode(void)
{
	//上一次模式状态量
	uint8_t PreBBFlag=0;
	
	//暂存当前的模式状态量
	PreBBFlag = DF.BBFlag;
	switch(DF.BBFlag)
	{
		//NA
		case  NA :		
		{
		  if(CtrValue.Voref < ((SADC.VinAvg*3277)>>12))//vout<0.8*vin
				DF.BBFlag = Buck;//buck mode
			else if(CtrValue.Voref > ((SADC.VinAvg*4915)>>12))//vout>1.2*vin
				DF.BBFlag = Boost;//boost mode
			else
				DF.BBFlag = Mix;//buck-boost mode
			break;
		}
		//BUCK模式
		case  Buck :		
		{
			if(CtrValue.Voref > ((SADC.VinAvg*4915)>>12))//vout>1.2*vin
				DF.BBFlag = Boost;//boost mode
			else if(CtrValue.Voref >((SADC.VinAvg*3482)>>12))//1.2*vin>vout>0.85*vin
				DF.BBFlag = Mix;//buck-boost mode
			break;
		}
		//Boost模式
		case  Boost :		
		{
			if(CtrValue.Voref < ((SADC.VinAvg*3277)>>12))//vout<0.8*vin
				DF.BBFlag = Buck;//buck mode
			else if(CtrValue.Voref < ((SADC.VinAvg*4710)>>12))//0.8*vin<vout<1.15*vin
				DF.BBFlag = Mix;//buck-boost mode
			break;
		}
		//Mix模式
		case  Mix :		
		{
		  if(CtrValue.Voref < ((SADC.VinAvg*3277)>>12))//vout<0.8*vin
				DF.BBFlag = Buck;//buck mode
			else if(CtrValue.Voref > ((SADC.VinAvg*4915)>>12))//vout>1.2*vin
				DF.BBFlag = Boost;//boost mode
			break;
		}	
	}	
	//当模式发生变换时（上一次和这一次不一样）,则标志位置位
	if(PreBBFlag==DF.BBFlag)
		DF.BBModeChange =0;
	else
		DF.BBModeChange =1;
	
}
/** ===================================================================
**     Funtion Name :void MX_OLED_Init(void)
**     Description :OLED初始化函数

**     Parameters  :
**     Returns     :
** ===================================================================*/
void MX_OLED_Init(void)
{
	//初始化OLED
	OLED_Init();
	OLED_CLS();
	OLED_ShowStr(25,0,"BUCK-BOOST",2);
	OLED_ShowStr(0,2,"State:",2);
	OLED_ShowStr(0,4,"Vout:",2);
	OLED_ShowStr(68,4,".",2);
	OLED_ShowStr(95,4,"V",2);
	OLED_ShowStr(0,6,"Iout:",2);
	OLED_ShowStr(68,6,".",2);
	OLED_ShowStr(95,6,"A",2);
	OLED_ON(); 	
}

/** ===================================================================
**     Funtion Name :void OLEDShow(void)
**     Description : OLED显示函数		 
**     显示运行模式-BUCK MODE,BOOST MODE,MIX MODE
**     显示状态：IDEL,RISISING,RUNNING,ERROR
**     显示输出电压：两位小数
**     显示输出电流：两位小数
**     Parameters  :
**     Returns     :
** ===================================================================*/
void OLEDShow(void)
{
	u8 Vtemp[4]={0,0,0,0};
	u8 Itemp[4]={0,0,0,0};
	uint32_t VoutT=0,IoutT=0;
	//uint32_t VinT=0,IinT=0,VadjT=0;
	static uint16_t BBFlagTemp=10,SMFlagTemp=10;
	
	//将采样值转换成实际值，并扩大100倍(显示屏幕带小数点)
	VoutT = SADC.VoutAvg*6800>>12;
	IoutT = 1100-(SADC.IoutAvg*2200>>12);
	//VinT = SADC.VinAvg*6800>>12;
	//IinT = 1100-(SADC.IinAvg*2200>>12);
	//VadjT = CtrValue.Voref*6800>>12;
	
	//分别保存实际电压和电流的每一位，小数点后两位保留
	//输出电压
	Vtemp[0] = (u8)(VoutT/1000);
	Vtemp[1] = (u8)((VoutT-(uint8_t)Vtemp[0]*1000)/100);
	Vtemp[2] = (u8)((VoutT-(uint16_t)Vtemp[0]*1000-(uint16_t)Vtemp[1]*100)/10);
	Vtemp[3] = (u8)(VoutT-(uint16_t)Vtemp[0]*1000-(uint16_t)Vtemp[1]*100-(uint16_t)Vtemp[2]*10);	
	//输入电压
/*	Vtemp[0] = (u8)(VinT/1000);
	Vtemp[1] = (u8)((VinT-(uint8_t)Vtemp[0]*1000)/100);
	Vtemp[2] = (u8)((VinT -(uint16_t)Vtemp[0]*1000-(uint16_t)Vtemp[1]*100)/10);
	Vtemp[3] = (u8)(VinT-(uint16_t)Vtemp[0]*1000-(uint16_t)Vtemp[1]*100-(uint16_t)Vtemp[2]*10);	*/
	//输出电流
	Itemp[0] = (u8)(IoutT/1000);
	Itemp[1] = (u8)((IoutT-(uint8_t)Itemp[0]*1000)/100);
	Itemp[2] = (u8)((IoutT-(uint16_t)Itemp[0]*1000-(uint16_t)Itemp[1]*100)/10);
	Itemp[3] = (u8)(IoutT-(uint16_t)Itemp[0]*1000-(uint16_t)Itemp[1]*100-(uint16_t)Itemp[2]*10);
	//输入电流
/*	Itemp[0] = (u8)(IinT/1000);
	Itemp[1] = (u8)((IinT-(uint8_t)Itemp[0]*1000)/100);
	Itemp[2] = (u8)((IinT-(uint16_t)Itemp[0]*1000-(uint16_t)Itemp[1]*100)/10);
	Itemp[3] = (u8)(IinT-(uint16_t)Itemp[0]*1000-(uint16_t)Itemp[1]*100-(uint16_t)Itemp[2]*10); */
	//参考电压
/*	Vtemp[0] = (u8)(VadjT/1000);
	Vtemp[1] = (u8)((VadjT-(uint8_t)Vtemp[0]*1000)/100);
	Vtemp[2] = (u8)((VadjT-(uint16_t)Vtemp[0]*1000-(uint16_t)Vtemp[1]*100)/10);
	Vtemp[3] = (u8)(VadjT-(uint16_t)Vtemp[0]*1000-(uint16_t)Vtemp[1]*100-(uint16_t)Vtemp[2]*10);	*/
	
	//如果运行模式有变化，则更改屏幕
	if(BBFlagTemp!= DF.BBFlag)
	{
		//暂存标志位
		BBFlagTemp = DF.BBFlag;
		//显示运行模式
		switch(DF.BBFlag)
		{
			//NA
			case  NA :		
			{
				OLED_ShowStr(25,0,"MODE:*NA* ",2);
				break;
			}
			//BUCK模式
			case  Buck :		
			{
				OLED_ShowStr(25,0,"MODE:BUCK ",2);
				break;
			}
			//Boost模式
			case  Boost :		
			{
				OLED_ShowStr(25,0,"MODE:BOOST",2);
				break;
			}
			//Mix模式
			case  Mix :		
			{
				OLED_ShowStr(25,0,"MODE:MIX  ",2);
				break;
			}
		}
	}
	
	//电源工作状态有变换，更新屏幕
	if(SMFlagTemp!= DF.SMFlag)
	{	
		SMFlagTemp = DF.SMFlag;
		//显示电源工作状态
		switch(DF.SMFlag)
		{
			//初始化状态
			case  Init :
			{
				OLED_ShowStr(55,2,"Init  ",2);
				break;
			}
			//等待状态
			case  Wait :
			{
				OLED_ShowStr(55,2,"Waiting",2);
				break;
			}
			//软启动状态
			case  Rise :
			{
				OLED_ShowStr(55,2,"Rising",2);
				break;
			}
			//运行状态
			case  Run :
			{
				OLED_ShowStr(55,2,"Running",2);
				break;
			}
			//故障状态
			case  Err :
			{
				OLED_ShowStr(55,2,"Error  ",2);
				break;
			}
		}	
	}
	
	//显示电压电流每一位
	OLEDShowData(50,4,Vtemp[0]);
	OLEDShowData(60,4,Vtemp[1]);
	OLEDShowData(75,4,Vtemp[2]);
	OLEDShowData(85,4,Vtemp[3]);
	OLEDShowData(50,6,Itemp[0]);
	OLEDShowData(60,6,Itemp[1]);
	OLEDShowData(75,6,Itemp[2]);
	OLEDShowData(85,6,Itemp[3]);
}






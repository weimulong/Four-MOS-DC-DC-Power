#ifndef __FUNCTION_H
#define __FUNCTION_H	 

#include "stm32f3xx_hal.h"
#include "hrtim.h"
#include "oled.h"
#include "adc.h"

extern uint16_t ADC1_RESULT[4];
extern struct  _ADI SADC;
extern struct  _Ctr_value  CtrValue;
extern struct  _FLAG    DF;
extern uint16_t OLEDShowCnt;

//函数声明
void ADCSample(void);
void StateM(void);
void StateMInit(void);
void StateMWait(void);
void StateMRise(void);
void StateMRun(void);
void StateMErr(void);
void ValInit(void);
void VrefGet(void);
void ShortOff(void);
void SwOCP(void);
void VoutSwOVP(void);
void VinSwUVP(void);
void VinSwOVP(void);
void LEDShow(void);
void KEYFlag(void);
void BBMode(void);
void OLEDShow(void);
void MX_OLED_Init(void);

/*****************************故障类型*****************/
#define     F_NOERR      			0x0000//无故障
#define     F_SW_VIN_UVP  		0x0001//输入欠压
#define     F_SW_VIN_OVP    	0x0002//输入过压
#define     F_SW_VOUT_UVP  		0x0004//输出欠压
#define     F_SW_VOUT_OVP    	0x0008//输出过压
#define     F_SW_IOUT_OCP    	0x0010//输出过流
#define     F_SW_SHORT  			0x0020//输出短路

#define MIN_BUKC_DUTY	80//BUCK最小占空比
#define MAX_BUCK_DUTY 3809//BUCK最大占空比，93%*Q12
#define	MAX_BUCK_DUTY1 3277//MIX模式下 BUCK固定占空比80%
#define MIN_BOOST_DUTY 80//BOOST最小占空比
#define MIN_BOOST_DUTY1 283//BOOST最小占空7%
#define MAX_BOOST_DUTY	2662//最大占空比 65%最大占空比
#define MAX_BOOST_DUTY1	3809//BUCK最大占空比，93%*Q12

//采样变量结构体
struct _ADI
{
	int32_t   Iout;//输出电流
	int32_t   IoutAvg;//输出电流平均值
	int32_t   Vout;//输出电电压
	int32_t   VoutAvg;//输出电电压平均值
	int32_t   Iin;//输出电流
	int32_t   IinAvg;//输出电流平均值
	int32_t   Vin;//输出电电压
	int32_t   VinAvg;//输出电电压平均值
	int32_t   Vadj;//滑动变阻器电压值
	int32_t   VadjAvg;//滑动变阻器电压平均值
};

#define CAL_VOUT_K	4101//Q12输入电压矫正K值
#define CAL_VOUT_B	49//Q12输入电压矫正B值
#define CAL_IOUT_K	4096//Q12输出电流矫正K值
#define CAL_IOUT_B	105//Q12输入电流矫正B值
#define CAL_VIN_K	4068//Q12输出电压矫正K值
#define CAL_VIN_B	59//Q12输出电压矫正B值
#define CAL_IIN_K	4096//Q12输入电流矫正K值
#define CAL_IIN_B	74//Q12输出电流矫正B值

struct  _Ctr_value
{
	int32_t		Voref;//参考电压
	int32_t		Ioref;//参考电流
	int32_t		ILimit;//限流参考电流
	int16_t		BUCKMaxDuty;//Buck最大占空比
	int16_t		BoostMaxDuty;//Boost最大占空比
	int16_t		BuckDuty;//Buck控制占空比
	int16_t		BoostDuty;//Boost控制占空比
	int32_t		Ilimitout;//电流环输出
};

//标志位定义
struct  _FLAG
{
	uint16_t	SMFlag;//状态机标志位
	uint16_t	CtrFlag;//控制标志位
	uint16_t  ErrFlag;//故障标志位
	uint8_t	BBFlag;//运行模式标志位，BUCK模式，BOOST模式，MIX混合模式	
	uint8_t PWMENFlag;//启动标志位	
	uint8_t KeyFlag1;//按键标志位
	uint8_t KeyFlag2;//按键标志位	
	uint8_t BBModeChange;//工作模式切换标志位
};

//状态机枚举量
typedef enum
{
    Init,//初始化
    Wait,//空闲等待
    Rise,//软启
    Run,//正常运行
    Err//故障
}STATE_M;

//状态机枚举量
typedef enum
{
    NA,//未定义
		Buck,//BUCK模式
    Boost,//BOOST模式
    Mix//MIX混合模式
}BB_M;

//软启动枚举变量
typedef enum
{
	SSInit,//软启初始化
	SSWait,//软启等待
	SSRun//开始软启
 } SState_M;

#define setRegBits(reg, mask)   (reg |= (unsigned int)(mask))
#define clrRegBits(reg, mask)  	(reg &= (unsigned int)(~(unsigned int)(mask)))
#define getRegBits(reg, mask)   (reg & (unsigned int)(mask))
#define getReg(reg)           	(reg)

#define CCMRAM  __attribute__((section("ccmram")))

#endif

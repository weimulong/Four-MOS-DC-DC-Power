# Four-MOS DC-DC Power

基于 STM32F334 的四开关双向同步 Buck-Boost DC-DC 变换器参考工程，包含嵌入式控制固件、Keil MDK-ARM 工程以及原理图/PCB 设计工程。

> [!WARNING]
> 本项目涉及最高 48 V 输入、最高 50 V 输出和较大功率电力电子电路。错误的接线、参数或控制逻辑可能损坏器件并造成人身伤害。请在具备必要电气安全知识、限流电源和保护设备的条件下使用。仓库中的参数为项目设计指标，并非第三方测试认证结果。

## 项目概述

功率级采用四开关同步 Buck-Boost 拓扑，可根据输入、输出电压关系工作于 Buck、Boost 或 Buck-Boost（混合）模式。STM32F334 使用 HRTIM 产生互补 PWM，通过 ADC 采集输入/输出电压与电流，并在定时中断中完成数字闭环控制和状态管理。
<img width="653" height="245" alt="90b971d7-41df-410b-a70a-efe5eb92f29e" src="https://github.com/user-attachments/assets/c8fe5c8d-70c9-472d-bf77-0ba6c5097d39" />

固件主要包含：

- Buck、Boost 和 Buck-Boost 模式控制
- 电压环/电流环 PI、PID 及 Type-III 补偿代码
- HRTIM PWM 生成与占空比更新
- ADC + DMA 电压、电流采样
- 输入欠压/过压、输出过压/过流及短路保护逻辑
- OLED 状态显示
- 基于定时中断的运行状态机

## 项目设计参数

| 项目 | 标称值 |
| --- | --- |
| 主控 | STM32F334 |
| 拓扑 | 四开关同步 Buck-Boost |
| 输入电压 | 12–48 VDC |
| 输出电压范围 | 5–50 VDC |
| 最大输出电流 | 2 A |
| 最大输出功率 | 100 W |
| 开关频率 | 200 kHz |
| 调节方式 | 滑动电位器 |
| 烧录接口 | SWD |

以上为本项目实物对应的设计参数。实际可持续输出能力仍取决于输入电压、输出电压、元器件、PCB、散热、控制参数及测试条件；输出电压、电流和功率上限需要同时满足。

## 目录结构

```text
.
├── firmware/
│   ├── Core/                       # 应用代码与外设初始化
│   ├── Drivers/                    # STM32CubeF3 依赖安装说明
│   └── MDK-ARM/                    # Keil 工程与启动文件
├── hardware/
│   └── eda/                        # 原理图与 PCB 工程（.epro）
├── .gitignore
├── README.md
└── THIRD_PARTY_NOTICES.md
```

原压缩包内的 PDF 报告、规格书、编译结果、个人 IDE 配置，以及可从官方 STM32CubeF3 获取的 CMSIS/HAL 厂商代码未收录。

## 固件代码导览

建议首先阅读以下文件：

- `firmware/Core/Src/main.c`：系统和外设初始化
- `firmware/Core/Src/stm32f3xx_it.c`：HRTIM 与 TIM3 中断控制流程
- `firmware/Core/Src/CtlLoop.c`：数字闭环补偿和 PWM 更新
- `firmware/Core/Src/function.c`：采样、保护、工作模式及状态机
- `firmware/Core/Src/hrtim.c`：高分辨率定时器配置
- `firmware/Core/Src/adc.c`：ADC 采样配置

部分源文件保留了原始工程的传统中文编码。若注释在编辑器中显示异常，可尝试以 GBK/GB2312 编码打开；为避免改变程序内容，本仓库未批量转换源文件编码。

## 编译与烧录

1. 安装支持 STM32F3 系列的 Keil MDK-ARM 和对应器件包。
2. 按照 `firmware/Drivers/README.md` 准备 STM32CubeF3 的 CMSIS/HAL 依赖。
3. 打开 `firmware/MDK-ARM/Buck-Boost-Mode-VLoop-PID-BiDir.uvprojx`。
4. 检查目标器件和编译器版本；原工程目标为 STM32F334x8 系列。
5. 编译工程并通过 SWD 调试器下载。
6. 首次上电请使用限流电源，并在确认互补 PWM、死区和采样信号正确后再连接功率级。

本次整理只核对了工程引用文件是否齐全，未在 Keil 环境和实际硬件上重新编译或验证。

## 硬件工程

`hardware/eda/` 中的 `.epro` 文件为 EDA 工程归档，可使用兼容的立创 EDA/嘉立创 EDA 工具导入。打开后请重点复核：

- MOSFET 栅极驱动和死区时间
- 电流采样极性与量程
- 输入/输出电压采样比例
- 功率回路走线、电流能力和散热
- 保护阈值与实际器件耐压

## 来源、版权与使用范围

本仓库根据现有工程资料整理，部分代码和设计内容可能来自原开发者。仓库根目录未声明统一的开源许可证，也不表示对所有内容进行重新授权。STM32 HAL/CMSIS 依赖需从 STMicroelectronics/Arm 的官方发行包取得，并受其原始许可条款约束，详见 `THIRD_PARTY_NOTICES.md`。

在复制、修改、商业使用或重新发布前，请自行确认各部分的版权归属与许可条件。本仓库主要用于学习、研究和工程资料整理。

## 当前状态

- [x] 固件源码整理
- [x] Keil 工程文件整理
- [x] 原理图/PCB 工程归档
- [x] 自动生成文件清理

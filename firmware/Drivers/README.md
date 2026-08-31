# STM32CubeF3 dependencies

Vendor CMSIS and HAL sources are intentionally not redistributed in this repository. Obtain a compatible STM32CubeF3 package from STMicroelectronics, then copy or link these directories into this folder:

```text
Drivers/
├── CMSIS/
│   ├── Core/Include/
│   └── Device/ST/STM32F3xx/Include/
└── STM32F3xx_HAL_Driver/
    ├── Inc/
    └── Src/
```

The Keil project references the following HAL source modules:

- Core, Cortex and EXTI
- ADC and ADC extensions
- DMA
- Flash and Flash extensions
- GPIO
- HRTIM
- I2C and I2C extensions
- Power and power extensions
- RCC and RCC extensions
- TIM and TIM extensions

Use the STM32F334x8 device headers and keep the vendor license files from the selected STM32CubeF3 release. Because the original project dates from 2021, newer packages or compiler versions may require minor compatibility changes.

/****************************************************************************
 * boards/arm/stm32f1/m144z-m3/include/board.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/* ===========================================================================
 * 【本文件是什么】
 *   板级(board-level)公共头文件，描述“正点原子 M144Z-M3 Mini Board”这块特定
 *   电路板上的板级参数：时钟树、用户按键/LED、外设引脚复用。
 *
 * 【谁会包含它 / 宏从哪来又到哪去】
 *   configure.sh 配置本板时会建立软链：
 *       nuttx/include/arch/board  ->  boards/arm/stm32f1/m144z-m3/include
 *   于是芯片层代码统一用 <arch/board/board.h> 就能读到本文件。
 *
 *   - “时钟类”宏  被 arch/arm/src/stm32f1/stm32f10xxx_rcc.c 的
 *                  stm32_stdclockconfig() 在启动早期直接写入 RCC 寄存器；
 *                  其中引用到的 RCC_CFGR_xxx 位定义来自
 *                  arch/arm/src/stm32f1/hardware/stm32f10xxx_rcc.h。
 *   - “按键/LED”宏 被 boards/arm/stm32f1/m144z-m3/src/stm32_buttons.c、
 *                  stm32_autoleds.c、stm32_userleds.c 使用；
 *                  LED_xxx / BUTTON_xxx 的语义是 NuttX 通用板级约定
 *                  (见 include/nuttx/board.h)。
 *   - “引脚复用”宏 被 STM32 串口/SPI/I2C/定时器等驱动在初始化时
 *                  stm32_configgpio() 使用；带 _0/_1 后缀的候选映射来自
 *                  芯片 pinmap 头 stm32f103z_pinmap.h（由 CONFIG_ARCH_CHIP_
 *                  STM32F103ZE 经 hardware/stm32_pinmap.h 选中）。
 *
 * 【本板硬件速览（来源：正点原子 M144Z-M3 原理图）】
 *   MCU        : STM32F103ZET6  (Cortex-M3, 512KB Flash / 64KB SRAM)
 *   HSE 晶振   : 8MHz            -> PLL x9 -> SYSCLK 72MHz
 *   调试串口   : USART1 (PA9/PA10) 经板载 CH340C 转 USB
 *   用户 LED   : LED0 红 = PB5，LED1 绿 = PE5   (低电平点亮)
 *   用户按键   : KEY0 = PE4                      (低电平表示按下，板上上拉)
 * ===========================================================================
 */

#ifndef __BOARDS_ARM_STM32F1_M144Z_M3_INCLUDE_BOARD_H
#define __BOARDS_ARM_STM32F1_M144Z_M3_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#ifndef __ASSEMBLY__
#  include <stdint.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clocking *****************************************************************/

/* HSE(高速外部时钟)晶振频率。
 *   含义：告诉时钟驱动板上外接晶振是多少 Hz，后面所有频率都由它推导。
 *   来源：正点原子 M144Z-M3 原理图，Y1 为 8MHz 无源晶振。
 *   使用：stm32f10xxx_rcc.c 中做 PLL 输入频率、Flash 等待周期等计算；
 *         同时 arch/arm/include/stm32f1/stm32f103xx.h 会据此校验上限。
 */

#define STM32_BOARD_XTAL        8000000ul

/* PLL 配置：PLL 输入 = HSE / 1，倍频系数 = 9 => PLL 输出 = 8MHz x 9 = 72MHz
 * (72MHz 是 STM32F103 的最高主频)
 */

/* PLL 时钟源选择位 (RCC_CFGR.PLLSRC)。
 *   RCC_CFGR_PLLSRC 置位 = 选 HSE 作为 PLL 输入；清零则为 HSI/2。
 *   位定义来源：hardware/stm32f10xxx_rcc.h
 *   写入位置  ：stm32f10xxx_rcc.c stm32_stdclockconfig() -> RCC_CFGR
 */

#define STM32_CFGR_PLLSRC       RCC_CFGR_PLLSRC

/* HSE 进入 PLL 前的分频位 (RCC_CFGR.PLLXTPRE)。
 *   0 = HSE 不分频直接进 PLL(本板)；1 = HSE/2 再进 PLL。
 *   本板 HSE=8MHz、需要 72MHz，故不分频。
 */

#define STM32_CFGR_PLLXTPRE     0

/* PLL 倍频系数 (RCC_CFGR.PLLMUL)。
 *   RCC_CFGR_PLLMUL_CLKx9 表示 x9；取值来源：hardware/stm32f10xxx_rcc.h。
 *   8MHz x 9 = 72MHz。
 */

#define STM32_CFGR_PLLMUL       RCC_CFGR_PLLMUL_CLKx9

/* PLL 输出频率(推导值，供下面其它宏引用)。= 9 * 8MHz = 72MHz */

#define STM32_PLL_FREQUENCY     (9*STM32_BOARD_XTAL)

/* 选择 PLL 输出作为系统时钟 SYSCLK */

/* 系统时钟切换位 (RCC_CFGR.SW)：选 PLL 作为 SYSCLK 源。
 *   写入位置：stm32_stdclockconfig() 在切换 SYSCLK 时写 RCC_CFGR.SW。
 */

#define STM32_SYSCLK_SW         RCC_CFGR_SW_PLL

/* 系统时钟状态位 (RCC_CFGR.SWS)：与上面 SW 对应的“已生效”标志。
 *   使用：驱动写完 SW 后轮询 SWS == 此值，确认切换完成。
 */

#define STM32_SYSCLK_SWS        RCC_CFGR_SWS_PLL

/* SYSCLK 实际频率 = PLL 频率 = 72MHz。是内核和各总线频率推导的根。 */

#define STM32_SYSCLK_FREQUENCY  STM32_PLL_FREQUENCY

/* AHB 总线时钟 HCLK = SYSCLK (不分频，72MHz) */

/* AHB 预分频位 (RCC_CFGR.HPRE)：RCC_CFGR_HPRE_SYSCLK = 不分频。
 *   HCLK 供 Cortex-M3 内核、SysTick、存储器、DMA、GPIO 端口时钟等。
 */

#define STM32_RCC_CFGR_HPRE     RCC_CFGR_HPRE_SYSCLK
#define STM32_HCLK_FREQUENCY    STM32_PLL_FREQUENCY

/* APB2 总线时钟 PCLK2 = HCLK (不分频，72MHz) */

/* APB2 预分频位 (RCC_CFGR.PPRE2)：RCC_CFGR_PPRE2_HCLK = 不分频。
 *   APB2 上挂：GPIO、USART1、SPI1、ADC1/2/3、TIM1、TIM8、EXTI/AFIO 等。
 */

#define STM32_RCC_CFGR_PPRE2    RCC_CFGR_PPRE2_HCLK
#define STM32_PCLK2_FREQUENCY   STM32_HCLK_FREQUENCY

/* APB2 上的高级定时器 TIM1、TIM8 的输入时钟。
 *   STM32 规则：APBx 预分频 = 1 时，该总线上定时器时钟 = PCLKx。
 *   这里 = PCLK2 = 72MHz。定时器驱动用它计算 PSC/ARR。
 */

#define STM32_TIM1_CLKIN   (STM32_PCLK2_FREQUENCY)
#define STM32_TIM8_CLKIN   (STM32_PCLK2_FREQUENCY)

/* APB1 总线时钟 PCLK1 = HCLK / 2 (36MHz) */

/* APB1 预分频位 (RCC_CFGR.PPRE1)：RCC_CFGR_PPRE1_HCLKd2 = 二分频。
 *   必须分频：APB1 频率上限为 36MHz，而 HCLK 已是 72MHz。
 *   APB1 上挂：USART2/3、UART4/5、I2C1/2、SPI2/3、TIM2~7、CAN1、USB、
 *              bxCAN、DAC、PWR、BKP 等。
 */

#define STM32_RCC_CFGR_PPRE1    RCC_CFGR_PPRE1_HCLKd2
#define STM32_PCLK1_FREQUENCY   (STM32_HCLK_FREQUENCY/2)

/* APB1 上的通用/基本定时器 TIM2~TIM7 的输入时钟。
 *   STM32 规则：APBx 预分频 != 1 时，该总线上定时器时钟 = 2 x PCLKx。
 *   这里 = 2 x 36MHz = 72MHz（与 APB2 定时器一致）。
 */

#define STM32_TIM2_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM3_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM4_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM5_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM6_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM7_CLKIN   (2*STM32_PCLK1_FREQUENCY)

/* USB 时钟预分频位 (RCC_CFGR.USBPRE)。
 *   USB 全速外设需要精确的 48MHz 时钟。
 *   0 = PLL / 1.5  => 72MHz / 1.5 = 48MHz（本板，因为 PLL 正好 72MHz）
 *   1 = PLL / 1    => 仅当 PLL = 48MHz 时使用
 *   写入位置：stm32f10xxx_rcc.c 配置 RCC_CFGR 时。
 */

#define STM32_CFGR_USBPRE       0

/* BUTTON definitions *******************************************************/

/* 本板只有一个用户按键 KEY0，接在 PE4。
 * 低电平有效（按下 = 低电平，板上已对该脚上拉）。
 * 具体引脚编码(GPIO_BTN_xxx)在 src/m144z-m3.h 中定义，本文件只定义“逻辑编号”。
 */

/* 按键总数。被 stm32_buttons.c、apps/examples/buttons 以及
 * include/nuttx/board.h 的 board_button_* API 使用。
 */

#define NUM_BUTTONS       1

/* KEY0 的逻辑索引(从 0 开始)。board_buttons() 返回值中用位来表示各键状态。 */

#define BUTTON_USER1      0

/* KEY0 对应的位掩码：board_buttons() 返回值 & 此掩码 != 0 表示 KEY0 被按下。 */

#define BUTTON_USER1_BIT  (1 << BUTTON_USER1)

/* LED definitions **********************************************************/

/* 本板有两个用户 LED，均为低电平点亮
 * (LED 阳极经限流电阻接 3.3V，MCU 引脚拉低即导通点亮)：
 *
 *   LED0 - 红色，接 PB5
 *   LED1 - 绿色，接 PE5
 *
 * 引脚编码(GPIO_LED0 / GPIO_LED1)在 src/m144z-m3.h 中定义。
 */

/* LED 总数。被 stm32_userleds.c / stm32_autoleds.c 使用。 */

#define BOARD_NLEDS       2

/* 下面 8 个不是引脚，而是“OS 运行状态编码”。
 *
 * 当 CONFIG_ARCH_LEDS=y 时，NuttX 内核会在启动/运行的关键节点调用
 *     board_autoled_on(state) / board_autoled_off(state)
 * 由 src/stm32_autoleds.c 把这些状态码翻译成 LED0/LED1 的具体亮灭组合，
 * 从而用两颗灯把“系统活到哪一步 / 是否死机”显示出来。
 *
 * 状态码的取值范围是 NuttX 通用约定，含义由各板自行解释：
 */

#define LED_STARTED       0  /* __start() 已进入                 -> LED0 亮 */
#define LED_HEAPALLOCATE  1  /* 堆(内存管理)已初始化             -> LED1 亮 */
#define LED_IRQSENABLED   2  /* 中断已使能                       -> LED1 亮 */
#define LED_STACKCREATED  3  /* idle 任务栈就绪(进入多任务调度)  -> LED0 亮 */
#define LED_INIRQ         4  /* 正在中断处理中(进/出瞬间置位清位)-> LED0 灭 */
#define LED_SIGNAL        5  /* 正在信号处理中                   -> LED1 亮 */
#define LED_ASSERTION     6  /* 断言失败(assert)                 -> LED0+LED1 */
#define LED_PANIC         7  /* 内核 panic                       -> LED0/LED1 闪烁 */

/* Alternate function pin selections (auto-aliased for new pinmap) */

/* 【这一段在做什么】
 *   STM32F1 很多外设的引脚可以“重映射”，芯片 pinmap 头
 *   (arch/arm/src/stm32f1/hardware/stm32f103z_pinmap.h) 因此为每个外设功能
 *   提供带 _0 后缀的“默认映射”候选(受 CONFIG_STM32_xxx_REMAP 影响)。
 *   板级 board.h 的职责：从候选里选定一组，并取驱动使用的最终名字
 *   (GPIO_USART1_TX 等，去掉 _0 后缀)。
 *
 * 【GPIO_ADJUST_MODE(p, m) 的作用】
 *   定义在 arch/arm/src/common/stm32/stm32_gpio_m3m4_v1v2.h：
 *       ((p) & ~GPIO_MODE_MASK) | (m)
 *   即：把候选映射 p 里的“输出速度”字段替换为 m。
 *   pinmap 默认给的是 GPIO_MODE_2MHz；这里对【输出脚】(TX/SCK/MOSI/SDA/
 *   CH?OUT ...) 统一改成 GPIO_MODE_50MHz，以支持更高波特率 / 时钟频率。
 *   【输入脚】(RX/MISO/CH?IN/BKIN/ETR ...) 本身是输入模式、没有速度概念，
 *   直接使用候选值。
 */

/* USART1 -- PA9(TX) / PA10(RX)。板载 CH340C USB 转串口，用作 NSH 调试控制台。
 *   (未定义 CONFIG_STM32_USART1_REMAP，故走默认 PA9/PA10；重映射则为 PB6/PB7)
 */

#define GPIO_USART1_TX     GPIO_ADJUST_MODE(GPIO_USART1_TX_0, GPIO_MODE_50MHz)
#define GPIO_USART1_RX     GPIO_USART1_RX_0

/* USART2 -- PA2(TX) / PA3(RX)，由排针引出 */

#define GPIO_USART2_TX     GPIO_ADJUST_MODE(GPIO_USART2_TX_0, GPIO_MODE_50MHz)
#define GPIO_USART2_RX     GPIO_USART2_RX_0

/* USART3 -- PB10(TX) / PB11(RX)，由排针引出。
 *   注意：与下面 I2C2 占用同一对引脚，二者只能选其一。
 */

#define GPIO_USART3_TX     GPIO_ADJUST_MODE(GPIO_USART3_TX_0, GPIO_MODE_50MHz)
#define GPIO_USART3_RX     GPIO_USART3_RX_0

/* SPI1 -- PA4(NSS) / PA5(SCK) / PA6(MISO) / PA7(MOSI)，由排针引出 */

#define GPIO_SPI1_NSS      GPIO_ADJUST_MODE(GPIO_SPI1_NSS_0, GPIO_MODE_50MHz)
#define GPIO_SPI1_SCK      GPIO_ADJUST_MODE(GPIO_SPI1_SCK_0, GPIO_MODE_50MHz)
#define GPIO_SPI1_MISO     GPIO_ADJUST_MODE(GPIO_SPI1_MISO_0, GPIO_MODE_50MHz)
#define GPIO_SPI1_MOSI     GPIO_ADJUST_MODE(GPIO_SPI1_MOSI_0, GPIO_MODE_50MHz)

/* I2C1 -- PB6(SCL) / PB7(SDA)，由排针引出。
 *   (pinmap 中 I2C 引脚为 AFOD 开漏输出，需外部上拉电阻)
 */

#define GPIO_I2C1_SCL      GPIO_ADJUST_MODE(GPIO_I2C1_SCL_0, GPIO_MODE_50MHz)
#define GPIO_I2C1_SDA      GPIO_ADJUST_MODE(GPIO_I2C1_SDA_0, GPIO_MODE_50MHz)

/* I2C2 -- PB10(SCL) / PB11(SDA)，由排针引出。与 USART3 冲突，见上。 */

#define GPIO_I2C2_SCL      GPIO_ADJUST_MODE(GPIO_I2C2_SCL_0, GPIO_MODE_50MHz)
#define GPIO_I2C2_SDA      GPIO_ADJUST_MODE(GPIO_I2C2_SDA_0, GPIO_MODE_50MHz)

/* CAN1 -- PA11(RX) / PA12(TX)，由排针引出。
 *   注意：与下面 USB(device) 占用同一对引脚，二者互斥。
 */

#define GPIO_CAN1_RX       GPIO_CAN1_RX_0
#define GPIO_CAN1_TX       GPIO_ADJUST_MODE(GPIO_CAN1_TX_0, GPIO_MODE_50MHz)

/* USB (全速 device) -- PA11(DM) / PA12(DP)。
 *   USB 差分线固定引脚，不需要也不能调速度字段，直接用候选值。
 */

#define GPIO_USB_DM        GPIO_USB_DM_0
#define GPIO_USB_DP        GPIO_USB_DP_0

/* TIM1 (APB2 高级定时器) 各通道引脚。
 *   均取 pinmap 默认(不重映射)映射；输入脚(CH?IN/BKIN/ETR)用候选值，
 *   输出脚(CH?OUT/CH?NOUT，含互补输出 N)调到 50MHz。
 *   具体落到哪个引脚见 stm32f103z_pinmap.h(受 CONFIG_STM32_TIM1_?_REMAP 影响)。
 */

#define GPIO_TIM1_CH1IN     GPIO_TIM1_CH1IN_0
#define GPIO_TIM1_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH2IN     GPIO_TIM1_CH2IN_0
#define GPIO_TIM1_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH3IN     GPIO_TIM1_CH3IN_0
#define GPIO_TIM1_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH4IN     GPIO_TIM1_CH4IN_0
#define GPIO_TIM1_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH4OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_BKIN      GPIO_TIM1_BKIN_0     /* 刹车输入(Break)，输入脚 */
#define GPIO_TIM1_ETR       GPIO_TIM1_ETR_0      /* 外部触发输入(ETR)，输入脚 */
#define GPIO_TIM1_CH1NOUT   GPIO_ADJUST_MODE(GPIO_TIM1_CH1NOUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH2NOUT   GPIO_ADJUST_MODE(GPIO_TIM1_CH2NOUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH3NOUT   GPIO_ADJUST_MODE(GPIO_TIM1_CH3NOUT_0, GPIO_MODE_50MHz)

/* TIM2 (APB1 通用定时器) 各通道引脚。约定同 TIM1。 */

#define GPIO_TIM2_CH1IN     GPIO_TIM2_CH1IN_0
#define GPIO_TIM2_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM2_CH2IN     GPIO_TIM2_CH2IN_0
#define GPIO_TIM2_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM2_CH3IN     GPIO_TIM2_CH3IN_0
#define GPIO_TIM2_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM2_CH4IN     GPIO_TIM2_CH4IN_0
#define GPIO_TIM2_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH4OUT_0, GPIO_MODE_50MHz)

/* TIM3 (APB1 通用定时器) 各通道引脚。约定同 TIM1。 */

#define GPIO_TIM3_CH1IN     GPIO_TIM3_CH1IN_0
#define GPIO_TIM3_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM3_CH2IN     GPIO_TIM3_CH2IN_0
#define GPIO_TIM3_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM3_CH3IN     GPIO_TIM3_CH3IN_0
#define GPIO_TIM3_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM3_CH4IN     GPIO_TIM3_CH4IN_0
#define GPIO_TIM3_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH4OUT_0, GPIO_MODE_50MHz)

/* TIM4 (APB1 通用定时器) 各通道引脚。约定同 TIM1。 */

#define GPIO_TIM4_CH1IN     GPIO_TIM4_CH1IN_0
#define GPIO_TIM4_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM4_CH2IN     GPIO_TIM4_CH2IN_0
#define GPIO_TIM4_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM4_CH3IN     GPIO_TIM4_CH3IN_0
#define GPIO_TIM4_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM4_CH4IN     GPIO_TIM4_CH4IN_0
#define GPIO_TIM4_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH4OUT_0, GPIO_MODE_50MHz)

#endif /* __BOARDS_ARM_STM32F1_M144Z_M3_INCLUDE_BOARD_H */

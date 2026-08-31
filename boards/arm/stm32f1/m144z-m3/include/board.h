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

/* The M144Z-M3 board (正点原子 / ALIENTEK) has an 8MHz HSE crystal. */

#define STM32_BOARD_XTAL        8000000ul

/* PLL source is HSE/1, PLL multiplier is 9: PLL frequency is
 * 8MHz (XTAL) x 9 = 72MHz
 */

#define STM32_CFGR_PLLSRC       RCC_CFGR_PLLSRC
#define STM32_CFGR_PLLXTPRE     0
#define STM32_CFGR_PLLMUL       RCC_CFGR_PLLMUL_CLKx9
#define STM32_PLL_FREQUENCY     (9*STM32_BOARD_XTAL)

/* Use the PLL and set the SYSCLK source to be the PLL */

#define STM32_SYSCLK_SW         RCC_CFGR_SW_PLL
#define STM32_SYSCLK_SWS        RCC_CFGR_SWS_PLL
#define STM32_SYSCLK_FREQUENCY  STM32_PLL_FREQUENCY

/* AHB clock (HCLK) is SYSCLK (72MHz) */

#define STM32_RCC_CFGR_HPRE     RCC_CFGR_HPRE_SYSCLK
#define STM32_HCLK_FREQUENCY    STM32_PLL_FREQUENCY

/* APB2 clock (PCLK2) is HCLK (72MHz) */

#define STM32_RCC_CFGR_PPRE2    RCC_CFGR_PPRE2_HCLK
#define STM32_PCLK2_FREQUENCY   STM32_HCLK_FREQUENCY

/* APB2 timers 1 and 8 will receive PCLK2. */

#define STM32_TIM1_CLKIN   (STM32_PCLK2_FREQUENCY)
#define STM32_TIM8_CLKIN   (STM32_PCLK2_FREQUENCY)

/* APB1 clock (PCLK1) is HCLK/2 (36MHz) */

#define STM32_RCC_CFGR_PPRE1    RCC_CFGR_PPRE1_HCLKd2
#define STM32_PCLK1_FREQUENCY   (STM32_HCLK_FREQUENCY/2)

/* APB1 timers 2-7 will be twice PCLK1 */

#define STM32_TIM2_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM3_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM4_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM5_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM6_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_TIM7_CLKIN   (2*STM32_PCLK1_FREQUENCY)

/* USB divider -- Divide PLL clock by 1.5 */

#define STM32_CFGR_USBPRE       0

/* BUTTON definitions *******************************************************/

/* The M144Z-M3 board has one user button, KEY0, connected to PE4.
 * The key is active-low (pressed = low level, the pin is pulled up
 * on the board).
 */

#define NUM_BUTTONS       1

#define BUTTON_USER1      0
#define BUTTON_USER1_BIT  (1 << BUTTON_USER1)

/* LED definitions **********************************************************/

/* The M144Z-M3 board has two user LEDs, both active-low (the anode of
 * each LED is connected to 3.3V through a current limiting resistor, so
 * driving the pin low turns the LED on):
 *
 *   LED0 - red,  connected to PB5
 *   LED1 - green, connected to PE5
 */

#define BOARD_NLEDS       2

#define LED_STARTED       0  /* LED0 on */
#define LED_HEAPALLOCATE  1  /* LED1 on */
#define LED_IRQSENABLED   2  /* LED1 on */
#define LED_STACKCREATED  3  /* LED0 on */
#define LED_INIRQ         4  /* LED0 off */
#define LED_SIGNAL        5  /* LED1 on */
#define LED_ASSERTION     6  /* LED0 + LED1 */
#define LED_PANIC         7  /* LED0 / LED1 blinking */

/* Alternate function pin selections (auto-aliased for new pinmap) */

/* USART1 -- PA9/PA10, connected to the on-board CH340C USB serial */

#define GPIO_USART1_TX     GPIO_ADJUST_MODE(GPIO_USART1_TX_0, GPIO_MODE_50MHz)
#define GPIO_USART1_RX     GPIO_USART1_RX_0

/* USART2 -- PA2/PA3 (header) */

#define GPIO_USART2_TX     GPIO_ADJUST_MODE(GPIO_USART2_TX_0, GPIO_MODE_50MHz)
#define GPIO_USART2_RX     GPIO_USART2_RX_0

/* USART3 -- PB10/PB11 (header) */

#define GPIO_USART3_TX     GPIO_ADJUST_MODE(GPIO_USART3_TX_0, GPIO_MODE_50MHz)
#define GPIO_USART3_RX     GPIO_USART3_RX_0

/* SPI1 -- PA5/PA6/PA7 (header) */

#define GPIO_SPI1_NSS      GPIO_ADJUST_MODE(GPIO_SPI1_NSS_0, GPIO_MODE_50MHz)
#define GPIO_SPI1_SCK      GPIO_ADJUST_MODE(GPIO_SPI1_SCK_0, GPIO_MODE_50MHz)
#define GPIO_SPI1_MISO     GPIO_ADJUST_MODE(GPIO_SPI1_MISO_0, GPIO_MODE_50MHz)
#define GPIO_SPI1_MOSI     GPIO_ADJUST_MODE(GPIO_SPI1_MOSI_0, GPIO_MODE_50MHz)

/* I2C1 -- PB6/PB7 (header) */

#define GPIO_I2C1_SCL      GPIO_ADJUST_MODE(GPIO_I2C1_SCL_0, GPIO_MODE_50MHz)
#define GPIO_I2C1_SDA      GPIO_ADJUST_MODE(GPIO_I2C1_SDA_0, GPIO_MODE_50MHz)

/* I2C2 -- PB10/PB11 (header) */

#define GPIO_I2C2_SCL      GPIO_ADJUST_MODE(GPIO_I2C2_SCL_0, GPIO_MODE_50MHz)
#define GPIO_I2C2_SDA      GPIO_ADJUST_MODE(GPIO_I2C2_SDA_0, GPIO_MODE_50MHz)

/* CAN1 -- PA11/PA12 (header) */

#define GPIO_CAN1_RX       GPIO_CAN1_RX_0
#define GPIO_CAN1_TX       GPIO_ADJUST_MODE(GPIO_CAN1_TX_0, GPIO_MODE_50MHz)

/* USB -- PA11/PA12 (device) */

#define GPIO_USB_DM        GPIO_USB_DM_0
#define GPIO_USB_DP        GPIO_USB_DP_0

/* TIM1 */

#define GPIO_TIM1_CH1IN     GPIO_TIM1_CH1IN_0
#define GPIO_TIM1_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH2IN     GPIO_TIM1_CH2IN_0
#define GPIO_TIM1_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH3IN     GPIO_TIM1_CH3IN_0
#define GPIO_TIM1_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH4IN     GPIO_TIM1_CH4IN_0
#define GPIO_TIM1_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM1_CH4OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_BKIN      GPIO_TIM1_BKIN_0
#define GPIO_TIM1_ETR       GPIO_TIM1_ETR_0
#define GPIO_TIM1_CH1NOUT   GPIO_ADJUST_MODE(GPIO_TIM1_CH1NOUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH2NOUT   GPIO_ADJUST_MODE(GPIO_TIM1_CH2NOUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM1_CH3NOUT   GPIO_ADJUST_MODE(GPIO_TIM1_CH3NOUT_0, GPIO_MODE_50MHz)

/* TIM2 */

#define GPIO_TIM2_CH1IN     GPIO_TIM2_CH1IN_0
#define GPIO_TIM2_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM2_CH2IN     GPIO_TIM2_CH2IN_0
#define GPIO_TIM2_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM2_CH3IN     GPIO_TIM2_CH3IN_0
#define GPIO_TIM2_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM2_CH4IN     GPIO_TIM2_CH4IN_0
#define GPIO_TIM2_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM2_CH4OUT_0, GPIO_MODE_50MHz)

/* TIM3 */

#define GPIO_TIM3_CH1IN     GPIO_TIM3_CH1IN_0
#define GPIO_TIM3_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM3_CH2IN     GPIO_TIM3_CH2IN_0
#define GPIO_TIM3_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM3_CH3IN     GPIO_TIM3_CH3IN_0
#define GPIO_TIM3_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM3_CH4IN     GPIO_TIM3_CH4IN_0
#define GPIO_TIM3_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM3_CH4OUT_0, GPIO_MODE_50MHz)

/* TIM4 */

#define GPIO_TIM4_CH1IN     GPIO_TIM4_CH1IN_0
#define GPIO_TIM4_CH1OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH1OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM4_CH2IN     GPIO_TIM4_CH2IN_0
#define GPIO_TIM4_CH2OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH2OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM4_CH3IN     GPIO_TIM4_CH3IN_0
#define GPIO_TIM4_CH3OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH3OUT_0, GPIO_MODE_50MHz)
#define GPIO_TIM4_CH4IN     GPIO_TIM4_CH4IN_0
#define GPIO_TIM4_CH4OUT    GPIO_ADJUST_MODE(GPIO_TIM4_CH4OUT_0, GPIO_MODE_50MHz)

#endif /* __BOARDS_ARM_STM32F1_M144Z_M3_INCLUDE_BOARD_H */

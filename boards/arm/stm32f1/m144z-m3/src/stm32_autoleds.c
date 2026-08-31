/****************************************************************************
 * boards/arm/stm32f1/m144z-m3/src/stm32_autoleds.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <nuttx/debug.h>

#include <nuttx/board.h>
#include <arch/board/board.h>

#include "chip.h"
#include "arm_internal.h"
#include "stm32.h"
#include "m144z-m3.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Both LEDs are active-low: writing a low level turns the LED on. */

static inline void set_led0(bool v)
{
  ledinfo("Turn LED0 %s\n", v ? "on" : "off");
  stm32_gpiowrite(GPIO_LED0, !v);
}

static inline void set_led1(bool v)
{
  ledinfo("Turn LED1 %s\n", v ? "on" : "off");
  stm32_gpiowrite(GPIO_LED1, !v);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_autoled_initialize
 ****************************************************************************/

#ifdef CONFIG_ARCH_LEDS
void board_autoled_initialize(void)
{
  /* Configure LED GPIOs for output */

  stm32_configgpio(GPIO_LED0);
  stm32_configgpio(GPIO_LED1);
}

/****************************************************************************
 * Name: board_autoled_on
 ****************************************************************************/

void board_autoled_on(int led)
{
  switch (led)
    {
    case LED_STARTED:      /* LED0 on */
    case LED_STACKCREATED:
      set_led0(true);
      break;

    case LED_HEAPALLOCATE: /* LED1 on */
    case LED_IRQSENABLED:
    case LED_SIGNAL:
      set_led1(true);
      break;

    case LED_ASSERTION:    /* LED0 + LED1 */
    case LED_PANIC:
      set_led0(true);
      set_led1(true);
      break;
    }
}

/****************************************************************************
 * Name: board_autoled_off
 ****************************************************************************/

void board_autoled_off(int led)
{
  switch (led)
    {
    case LED_INIRQ:        /* LED0 off */
      set_led0(false);
      break;

    case LED_SIGNAL:       /* LED1 off */
      set_led1(false);
      break;

    case LED_ASSERTION:    /* LED0 + LED1 off */
    case LED_PANIC:
      set_led0(false);
      set_led1(false);
      break;
    }
}

#endif /* CONFIG_ARCH_LEDS */

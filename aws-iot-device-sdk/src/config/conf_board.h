/**
 * \file
 *
 * \brief SAM D21 Xplained Pro board configuration.
 *
 * Copyright (c) 2013-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#ifndef CONF_BOARD_H_INCLUDED
#define CONF_BOARD_H_INCLUDED



/*solid relay CLK and DATA pin define*/
#define SR_CLK_PIN                  PIN_PA18
#define SR_DATA_PIN                 PIN_PA19

//Red LED on GDC board
#define LED_R_PIN                 PIN_PB10
#define LED_R_ACTIVE              false
#define LED_R_INACTIVE            !LED_G_ACTIVE

//Greem LED on GDC board
#define LED_G_PIN                 PIN_PB11
#define LED_G_ACTIVE              false
#define LED_G_INACTIVE            !LED_G_ACTIVE

//Button S1
#define BTN_S1_PIN                   PIN_PA11
#define BTN_S1_ACTIVE                false
#define BTN_S1_INACTIVE              !BTN_S1_ACTIVE


//LAMP on GDC board
#define LAMP_ON_PWM4CTRL_MODULE     TCC2
#define LAMP_ON_PWM4CTRL_CHANNEL    0
#define LAMP_ON_PWM4CTRL_OUTPUT     0
#define LAMP_ON_PWM4CTRL_PIN        PIN_PA16E_TCC2_WO0
#define LAMP_ON_PWM4CTRL_MUX        MUX_PA16E_TCC2_WO0
#define LAMP_ON_PWM4CTRL_PINMUX     PINMUX_PA16E_TCC2_WO0

#define LAMP_SNS_INPUT              PIN_PA17

//Buzzer on GDC board
#define BUZZ_ON_PWM4CTRL_MODULE     TCC0
#define BUZZ_ON_PWM4CTRL_CHANNEL    2
#define BUZZ_ON_PWM4CTRL_OUTPUT     6
#define BUZZ_ON_PWM4CTRL_PIN        PIN_PB12F_TCC0_WO6
#define BUZZ_ON_PWM4CTRL_MUX        MUX_PB12F_TCC0_WO6
#define BUZZ_ON_PWM4CTRL_PINMUX     PINMUX_PB12F_TCC0_WO6

#define BUZZ_SENSOR_INPUT           PIN_PB14

#endif /* CONF_BOARD_H_INCLUDED */

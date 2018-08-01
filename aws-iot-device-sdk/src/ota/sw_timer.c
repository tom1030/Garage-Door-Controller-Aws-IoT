/**
 * \file
 *
 * \brief SW Timer component for the IoT(Internet of things) service.
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
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

#include "sw_timer.h"
#include <errno.h>

#ifndef __FREERTOS__
/** Tick count of timer. */
static uint32_t sw_timer_tick = 0;
#endif

#define FOREACH_HANDLER(module, h) for (struct sw_timer_handle *h = module_inst->handler; h != NULL; h = h->next)

/**
 * \brief TCC callback of SW timer.
 *
 * This function performs to the increasing the tick count.
 *
 * \param[in] module Instance of the TCC.
 */
#if SAM0
#ifndef __FREERTOS__

static void sw_timer_tcc_callback(struct tcc_module *const module)
{
	sw_timer_tick++;
}

#endif
#elif SAM
void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* Time has changed, refresh display */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
		sw_timer_tick++;
	}
}

#endif

void sw_timer_get_config_defaults(struct sw_timer_config *const config)
{
	Assert(config);
#ifdef __FREERTOS__  
    config->accuracy = portTICK_PERIOD_MS;
#else
    config->accuracy = 125;
#endif
	config->tcc_dev = 0;
	config->tcc_callback_channel = 0;
}

void sw_timer_init(struct sw_timer_module *const module_inst, struct sw_timer_config *const config)
{
#if SAM0
	struct tcc_config tcc_conf;
	struct tcc_module *tcc_module;
	Tcc *hw[] = TCC_INSTS;
#endif

	Assert(module_inst);
	Assert(config);
	Assert(config->tcc_dev < TCC_INST_NUM);
	Assert(config->tcc_callback_channel < TCC_NUM_CHANNELS);

	module_inst->accuracy = config->accuracy;

	module_inst->handler = NULL;
#if SAM0
//if define __FREERTOS__, use freeRTOS system task tick as sw timer tick
#ifndef __FREERTOS__    
	/* Start the TCC module. */
	tcc_module = &module_inst->tcc_inst;
	tcc_get_config_defaults(&tcc_conf, hw[config->tcc_dev]);
	tcc_conf.counter.period = system_cpu_clock_get_hz() / (1000 / config->accuracy);
	tcc_conf.counter.clock_prescaler = TCC_CLOCK_PRESCALER_DIV1;
	tcc_init(tcc_module, hw[config->tcc_dev], &tcc_conf);
	tcc_register_callback(tcc_module, sw_timer_tcc_callback, config->tcc_callback_channel + TCC_CALLBACK_CHANNEL_0);
	tcc_enable_callback(tcc_module, config->tcc_callback_channel + TCC_CALLBACK_CHANNEL_0);
#endif
#elif SAM
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, OSC_SLCK_32K_XTAL_HZ / (1000 / config->accuracy));

	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT)) {
	}
#endif
}

void sw_timer_enable(struct sw_timer_module *const module_inst)
{
#if SAM0
#ifndef __FREERTOS__
	struct tcc_module *tcc_module;
#endif
#endif

	Assert(module_inst);
#if SAM0
#ifndef __FREERTOS__
	tcc_module = &module_inst->tcc_inst;

	tcc_enable(tcc_module);
#endif
#elif SAM
	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_RTTINCIEN);
#endif
}

void sw_timer_disable(struct sw_timer_module *const module_inst)
{
#if SAM0
#ifndef __FREERTOS__
	struct tcc_module *tcc_module;
#endif
#endif

	Assert(module_inst);

#if SAM0
#ifndef __FREERTOS__
	tcc_module = &module_inst->tcc_inst;
	tcc_disable(tcc_module);
#endif
#elif SAM
	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN);
#endif
}

uint32_t sw_timer_register_callback(struct sw_timer_module *const module_inst,
		sw_timer_callback_t callback, void *context, uint32_t period)
{
	int index = 0;
	struct sw_timer_handle *handler, *prev = NULL;

	Assert(module_inst);

	FOREACH_HANDLER(module, _handler) {
		prev = _handler;
		index = prev->timer_id + 1;
	}

	handler = calloc(1, sizeof(struct sw_timer_handle));
	if (handler == NULL) {
		return -ENOMEM;
	}

	handler->callback = callback;
	handler->callback_enable = 0;
	handler->context = context;
	handler->period = period / module_inst->accuracy;
	handler->timer_id = index;
	handler->next = NULL;

	if (prev != NULL) {
		prev->next = handler;
	} else {
		module_inst->handler = handler;
	}

	return index;
}

void sw_timer_unregister_callback(struct sw_timer_module *const module_inst, uint32_t timer_id)
{
	struct sw_timer_handle *prev = NULL;

	Assert(module_inst);

	FOREACH_HANDLER(module, handler) {
		if (handler->timer_id == timer_id) {
			/* Found */
			if (prev == NULL) {
				/* This is first element in the list */
				module_inst->handler = handler->next;
			} else {
				prev->next = handler->next;
			}

			free(handler);
			break;
		}

		prev = handler;
	}
}

void sw_timer_enable_callback(struct sw_timer_module *const module_inst, uint32_t timer_id, uint32_t delay)
{
	Assert(module_inst);

	FOREACH_HANDLER(module, handler) {
		if (handler->timer_id == timer_id) {
			/* Found */
			handler->callback_enable = 1;
		#ifdef __FREERTOS__
		    uint32_t sw_timer_tick = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
		#endif
			handler->expire_time = sw_timer_tick + (delay / module_inst->accuracy);
			break;
		}
	}
}

void sw_timer_disable_callback(struct sw_timer_module *const module_inst, uint32_t timer_id)
{
	Assert(module_inst);

	FOREACH_HANDLER(module, handler) {
		if (handler->timer_id == timer_id) {
			/* Found */
			handler->callback_enable = 0;
			break;
		}
	}
}

void sw_timer_task(struct sw_timer_module *const module_inst)
{
	Assert(module_inst);

	FOREACH_HANDLER(module, handler) {
		if (handler->callback_enable) {
		#ifdef __FREERTOS__
		    uint32_t sw_timer_tick = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
		#endif
			if ((int)(handler->expire_time - sw_timer_tick) < 0 && handler->busy == 0) {
				/* Enter critical section. */
				handler->busy = 1;
				/* Timer was expired. */
				if (handler->period > 0) {
					handler->expire_time = sw_timer_tick + handler->period;
				} else {
					/* One shot. */
					handler->callback_enable = 0;
				}

				/* Call callback function. */
				handler->callback(module_inst, handler->timer_id, handler->context, handler->period);
				/* Leave critical section. */
				handler->busy = 0;
			}
		}
	}
}

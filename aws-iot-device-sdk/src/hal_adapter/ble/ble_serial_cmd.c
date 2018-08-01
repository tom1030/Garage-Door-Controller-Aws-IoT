/*
 * ble_serial_cmd.c
 *
 * Created: 12/10/2017 下午 4:56:52
 *  Author: NSC
 */ 

#include <asf.h>

#include "ble_serial_cmd.h"

SemaphoreHandle_t ble_rx_event = NULL;

void ble_isr(void){
	BaseType_t pxHigherPriorityTaskWoken;
	
	xSemaphoreGiveFromISR(ble_rx_event, &pxHigherPriorityTaskWoken);
}

void ble_register_isr(void){

	struct extint_chan_conf config_extint_chan;

	extint_chan_get_config_defaults(&config_extint_chan);
	config_extint_chan.gpio_pin = CONF_BLE_INT_PIN;
	config_extint_chan.gpio_pin_mux = CONF_BLE_INT_MUX;
	config_extint_chan.gpio_pin_pull = EXTINT_PULL_UP;
	config_extint_chan.detection_criteria = EXTINT_DETECT_FALLING;

	extint_chan_set_config(CONF_BLE_INT_EIC, &config_extint_chan);
	extint_register_callback(ble_isr, CONF_BLE_INT_EIC,
			EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_enable_callback(CONF_BLE_INT_EIC,
			EXTINT_CALLBACK_TYPE_DETECT);
	
	ble_rx_event = xSemaphoreCreateCounting(1, 0);
}

void ble_unregister_isr(void){
	extint_unregister_callback(ble_isr, CONF_BLE_INT_EIC,
			EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_disable_callback(CONF_BLE_INT_EIC,
			EXTINT_CALLBACK_TYPE_DETECT);
}



void ble_interrupt_ctrl(uint8_t u8Enable)
{
	if (u8Enable) {
		extint_chan_enable_callback(CONF_BLE_INT_EIC,
				EXTINT_CALLBACK_TYPE_DETECT);
	} else {
		extint_chan_disable_callback(CONF_BLE_INT_EIC,
				EXTINT_CALLBACK_TYPE_DETECT);
	}
}

void ble_int_config_output(void){
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	//BLE interrupt pin
	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(CONF_BLE_INT_OUTPUT_PIN, &pin_conf);
	port_pin_set_output_level(CONF_BLE_INT_OUTPUT_PIN, true);
}

/** UART module for debug. */
struct usart_module ble_uart_module;

/**
 *  Configure UART console.
 */
void ble_config_uart(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);

	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 9600;//115200;

	while (usart_init(&ble_uart_module,
				EDBG_CDC_MODULE, &usart_conf) != STATUS_OK) {
	}
	usart_enable(&ble_uart_module);
	
}


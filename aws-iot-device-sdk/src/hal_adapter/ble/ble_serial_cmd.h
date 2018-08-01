/*
 * ble_serial_cmd.h
 *
 * Created: 12/10/2017 下午 4:57:11
 *  Author: NSC
 */ 


#ifndef BLE_SERIAL_CMD_H_
#define BLE_SERIAL_CMD_H_


#ifdef __cplusplus
extern "C" {
#endif

/** BLE interrupt pin. */
#define CONF_BLE_INT_PIN			PIN_PA21A_EIC_EXTINT5
#define CONF_BLE_INT_MUX			MUX_PA21A_EIC_EXTINT5
#define CONF_BLE_INT_EIC			(5) 

#define CONF_BLE_INT_OUTPUT_PIN     PIN_PA21

extern SemaphoreHandle_t ble_rx_event;
extern struct usart_module ble_uart_module;

void ble_register_isr(void);
void ble_unregister_isr(void);
void ble_interrupt_ctrl(uint8_t u8Enable);
void ble_int_config_output(void);
void ble_config_uart(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_SERIAL_CMD_H_ */
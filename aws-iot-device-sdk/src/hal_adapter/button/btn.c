/*
 * btn.c
 *
 * Created: 24/7/2017 下午 3:02:44
 *  Author: NSC
 */ 

#include <asf.h>
#include "btn.h"

BtnStatus btn_status = {BTN_RELEASE,0};

BtnState btn_new_state(void){
	bool ret;
	uint32_t tick;
	uint32_t val = 0; //default 0
	BtnState btn = BTN_UNKNOW;

	ret = port_pin_get_input_level(BTN_S1_PIN);
 	tick = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
 	
	switch (btn_status.state){
		case BTN_PRESS:
			if(tick < btn_status.ts_change){  
				// incase tick 32 bit overflow and rollback
				val = 0xFFFFFFFF;
			}
			if(ret == BTN_S1_INACTIVE){  //release
				if((tick - btn_status.ts_change + val >= SHORT_PRESS_THRESHOLD_L)
						&&(tick - btn_status.ts_change + val <= SHORT_PRESS_THRESHOLD_H)){
						btn_status.state = BTN_SHORT_PRESS;
						btn_status.ts_change = tick;
						btn = BTN_SHORT_PRESS;
					}else{
						btn_status.state = BTN_RELEASE;
						btn_status.ts_change = tick;
					}
			}
			else{ //press
				if(tick - btn_status.ts_change + val >= LONG_PRESS_THRESHOLD){
					btn_status.state = BTN_LONG_PRESS;
					btn_status.ts_change = tick;
					btn = BTN_LONG_PRESS;
				}
			}
			break;
		case BTN_LONG_PRESS:
			if(ret == BTN_S1_INACTIVE){
				btn_status.state = BTN_RELEASE;
				btn_status.ts_change = tick;
			}
			break;
		case BTN_RELEASE:
		case BTN_SHORT_PRESS:
			if(ret == BTN_S1_ACTIVE){  //press
				btn_status.state = BTN_PRESS;
				btn_status.ts_change = tick;
			}
			break;
		default:
			break;
	}
	return btn;
}

 

/*
 * timer.c
 *
 * Created: 2/6/2017 下午 4:10:08
 *  Author: NSC
 */ 

#ifdef __cplusplus
extern "C" {
#endif

#include <asf.h>
#include "timer_platform.h"


bool has_timer_expired(Timer *timer) {
	return left_ms(timer) > 0 ? 0 : 1;
}

void countdown_ms(Timer *timer, uint32_t timeout) {
	uint32_t time;

	time = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
	
	timer->countdown_time.msec = (uint32_t)(time % 1000);
	timer->countdown_time.sec = (uint32_t)(time / 1000);
	
	timer->duration_time.msec = (uint32_t)(timeout % 1000);
	timer->duration_time.sec = (uint32_t)(timeout / 1000);
}

uint32_t left_ms(Timer *timer) {
	uint32_t time;
	uint32_t countdown_time,duration_time;
	uint32_t left_time = 0;

	time = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
	countdown_time = timer->countdown_time.sec * 1000 + timer->countdown_time.msec;
	duration_time = timer->duration_time.sec * 1000 + timer->duration_time.msec;

	if(0xffffffff - countdown_time >= duration_time){
		if((time >= countdown_time)&&(time < countdown_time + duration_time))
			left_time = duration_time - (time - countdown_time);
	}
	else{
		if(time >= countdown_time)
			left_time = duration_time - (time - countdown_time);
		else{
			if(0xffffffff - countdown_time + time + 1 < duration_time)
				left_time = duration_time - (0xffffffff - countdown_time + time + 1);
		}
	}
	return left_time;
}

void countdown_sec(Timer *timer, uint32_t timeout) {
	uint32_t time;
	
	time = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
	timer->countdown_time.msec = (uint32_t)(time % 1000);  //ms
	timer->countdown_time.sec = (uint32_t)(time / 1000);  //s
	
	timer->duration_time.sec = timeout;
	
}

void init_timer(Timer *timer) {
	timer->countdown_time = (time_val){0,0};
	timer->duration_time = (time_val){0,0};
}

#ifdef __cplusplus
}
#endif


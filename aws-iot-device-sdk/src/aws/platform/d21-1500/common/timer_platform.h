/*
 * timer_platform.h
 *
 * Created: 2/6/2017 下午 4:10:31
 *  Author: NSC
 */ 


#ifndef TIMER_PLATFORM_H_
#define TIMER_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
/**
 * @file timer_platform.h
 */

#include "timer_interface.h"

typedef struct{
	uint32_t sec;
	uint32_t msec;
}time_val;
/**
 * definition of the Timer struct. Platform specific
 */
struct Timer {
	time_val countdown_time;  //time when start countdown
	time_val duration_time;  //time duration to expire
};

#ifdef __cplusplus
}
#endif


#endif /* TIMER_PLATFORM_H_ */
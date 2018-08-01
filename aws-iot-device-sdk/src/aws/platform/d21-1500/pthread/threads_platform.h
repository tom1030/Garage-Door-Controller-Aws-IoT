/*
 * threads_platform.h
 *
 * Created: 2/6/2017 下午 4:11:31
 *  Author: NSC
 */ 

#include <asf.h>
#include "threads_interface.h"

#ifdef _ENABLE_THREAD_SUPPORT_
#ifndef THREADS_PLATFORM_H_
#define THREADS_PLATFORM_H_


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Mutex Type
 *
 * definition of the Mutex	 struct. Platform specific
 *
 */
struct _IoT_Mutex_t {
	SemaphoreHandle_t lock;
};

#ifdef __cplusplus
}
#endif


#endif /* THREADS_PLATFORM_H_ */
#endif /* _ENABLE_THREAD_SUPPORT_ */
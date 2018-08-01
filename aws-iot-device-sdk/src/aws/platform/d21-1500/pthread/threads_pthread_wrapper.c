/*
 * threads_pthread_wrapper.c
 *
 * Created: 2/6/2017 下午 4:11:47
 *  Author: NSC
 */ 

#include "threads_platform.h"
#ifdef _ENABLE_THREAD_SUPPORT_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the provided mutex
 *
 * Call this function to initialize the mutex
 *
 * @param IoT_Mutex_t - pointer to the mutex to be initialized
 * @return IoT_Error_t - error code indicating result of operation
 */
IoT_Error_t aws_iot_thread_mutex_init(IoT_Mutex_t *pMutex) {
    
	pMutex->lock = xSemaphoreCreateMutex();
	if(NULL == pMutex->lock)
		return MUTEX_INIT_ERROR;
	
	return SUCCESS;
}

/**
 * @brief Lock the provided mutex
 *
 * Call this function to lock the mutex before performing a state change
 * Blocking, thread will block until lock request fails
 *
 * @param IoT_Mutex_t - pointer to the mutex to be locked
 * @return IoT_Error_t - error code indicating result of operation
 */
IoT_Error_t aws_iot_thread_mutex_lock(IoT_Mutex_t *pMutex) {
	//wait until the lock is available
	if(pdTRUE != xSemaphoreTake(pMutex->lock,portMAX_DELAY)) {
		return MUTEX_LOCK_ERROR;
	}
	return SUCCESS;
}

/**
 * @brief Try to lock the provided mutex
 *
 * Call this function to attempt to lock the mutex before performing a state change
 * Non-Blocking, immediately returns with failure if lock attempt fails
 *
 * @param IoT_Mutex_t - pointer to the mutex to be locked
 * @return IoT_Error_t - error code indicating result of operation
 */
IoT_Error_t aws_iot_thread_mutex_trylock(IoT_Mutex_t *pMutex) {
	//don't wait if can't obtain the lock
	if(pdTRUE != xSemaphoreTake(pMutex->lock,0)) {
		return MUTEX_LOCK_ERROR;
	}

	return SUCCESS;
}

/**
 * @brief Unlock the provided mutex
 *
 * Call this function to unlock the mutex before performing a state change
 *
 * @param IoT_Mutex_t - pointer to the mutex to be unlocked
 * @return IoT_Error_t - error code indicating result of operation
 */
IoT_Error_t aws_iot_thread_mutex_unlock(IoT_Mutex_t *pMutex) {
	if(pdTRUE != xSemaphoreGive(pMutex->lock)) {
		return MUTEX_UNLOCK_ERROR;
	}

	return SUCCESS;
}

/**
 * @brief Destroy the provided mutex
 *
 * Call this function to destroy the mutex
 *
 * @param IoT_Mutex_t - pointer to the mutex to be destroyed
 * @return IoT_Error_t - error code indicating result of operation
 */
IoT_Error_t aws_iot_thread_mutex_destroy(IoT_Mutex_t *pMutex) {
	vSemaphoreDelete(pMutex->lock);
	if(NULL != pMutex->lock){
		return MUTEX_DESTROY_ERROR;
	}

	return SUCCESS;
}


#ifdef __cplusplus
}
#endif

#endif /* _ENABLE_THREAD_SUPPORT_ */


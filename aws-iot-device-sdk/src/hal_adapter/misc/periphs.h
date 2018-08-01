/*
 * periphs.h
 *
 * Created: 7/9/2017 下午 2:08:28
 *  Author: NSC
 */ 


#ifndef PERIPHS_H_
#define PERIPHS_H_


#ifdef __cplusplus
extern "C" {
#endif


#include "timer_interface.h"

/*status values define*/
#define TS_STATUS_VAL_NOT_ALLOW      0
#define TS_STATUS_VAL_OFF            1
#define TS_STATUS_VAL_ON_NODITHER    4
#define TS_STATUS_VAL_ON_DITHER      5
#define TS_STATUS_VAL_OVERCURRENT    7

/*switch commands define*/
#define TS_CMD_RESERVED     0
#define TS_CMD_OFF_IMME     1
#define TS_CMD_OFF_ZEROX    2
#define TS_CMD_ON_IMME      3
#define TS_CMD_ON_ZEROX     4
#define TS_CMD_ON_IMME_DI   5
#define TS_CMD_ON_ZEROX_DI  6
#define TS_CMD_HEART_BEAT   7
#define TS_CMD_SEND_PWR_8P   9
#define TS_CMD_SEND_PWR_16P  10
#define TS_CMD_SEND_PWR_32P  11
#define TS_CMD_SEND_PWR_64P  12
#define TS_CMD_POLL_STATE   15


//8 channels define for address 0000 ~ 0111
#define CH_NUMBER_A     0
#define CH_NUMBER_B     1
#define CH_NUMBER_C     2
#define CH_NUMBER_D     3
#define CH_NUMBER_E     4
#define CH_NUMBER_F     5
#define CH_NUMBER_G     6
#define CH_NUMBER_H     7

#define CH_NUMBER_A_ADDR     0   
#define CH_NUMBER_B_ADDR     1   //AD0 = VDD
#define CH_NUMBER_C_ADDR     2   //AD1 = VDD
#define CH_NUMBER_D_ADDR     3   //AD0 = VDD, AD1 = VDD
/*
#define CH_NUMBER_E_ADDR     0
#define CH_NUMBER_F_ADDR     0
#define CH_NUMBER_G_ADDR     0
#define CH_NUMBER_H_ADDR     0
*/

enum light_id{
    LED_R = 0,
	LED_G,
	LAMP,
	light_id_max = 3,
};

struct light_flash{
	bool onoff;    //enable or disable the light flashing task
	uint8_t case_bit;  //bit map for light on ('1') or off ('0') in 2s with 8 cases for 250ms each
	uint8_t case_going;   //0 ~ 7 indicate next case bit
	uint16_t duration_ms;
	Timer flash_tmr;
};

struct buzz_snd{
	bool onoff;    //enable or disable the buzzer sounding task
	uint16_t duration_ms;
	Timer snd_tmr;
};

extern struct light_flash light_task[];
extern struct buzz_snd buzz_task;

uint32_t send_ts_cmd (uint32_t ch, uint32_t cmd);
void pull_status_ch (uint32_t ch);
void solid_relay_init(void);

void lamp_onoff(bool onoff);
void lamp_init(void);

void buzz_onoff(bool onoff);
void buzz_init(void);


#ifdef __cplusplus
	}
#endif

#endif /* PERIPHS_H_ */

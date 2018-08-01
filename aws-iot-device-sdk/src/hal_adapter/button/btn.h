/*
 * btn.h
 *
 * Created: 24/7/2017 下午 3:02:56
 *  Author: NSC
 */ 

#ifndef BTN_H_
#define BTN_H_


#ifdef __cplusplus
extern "C" {
#endif

#define SHORT_PRESS_THRESHOLD_L 200  //min press time in ms
#define SHORT_PRESS_THRESHOLD_H 500  //max press time in ms
#define LONG_PRESS_THRESHOLD  3000  //min press time in ms


typedef enum{
	BTN_UNKNOW,
	BTN_PRESS,   //press
	BTN_SHORT_PRESS,  //release
	BTN_LONG_PRESS,  //press
	BTN_RELEASE,  //release
}BtnState;

typedef struct{
	BtnState state;      // current state
	uint32_t ts_change;  //timestamp last state change
}BtnStatus,*pBtnStatus;

BtnState btn_new_state(void);

#ifdef __cplusplus
}
#endif

#endif /* BTN_H_ */
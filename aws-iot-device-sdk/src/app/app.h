 /*
 * app.h
 *
 * Created: 14/6/2017 上午 11:37:28
 *  Author: NSC
 */ 


#ifndef APP_H_
#define APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VERSION (13)  // 1.3
#define FW_VERSION_STRING  "1.3"
int shadow_init(void);
int ota_check(void);
int dooropen_desire(const char* door,const uint8_t percent);

extern const char* FwOtaTarget[];

#ifdef __cplusplus
}
#endif
#endif /* APP_H_ */

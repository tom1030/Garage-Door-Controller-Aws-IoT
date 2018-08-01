 /*
 * winc.h
 *
 * Created: 6/6/2017 下午 3:05:12
 *  Author: NSC
 */ 


#ifndef WINC_H_
#define WINC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/include/m2m_wifi.h"
#include "socket/source/socket_internal.h"
#include "driver/include/m2m_ota.h"

/**
***  Macro
**/
#define WINC_MSG_SIZE 20       //WINC message max size in bytes 

/** IP address parsing. */
#define IPV4_BYTE(val, index)                ((val >> (index * 8)) & 0xFF)

/**
***  typedef
**/

typedef struct {
	uint8_t type;
	uint8_t len;
	uint8_t msg[WINC_MSG_SIZE];
}WincMsg,*pWincMsg;

typedef enum{
	IDLE = 0x00,
	//STATION
	STA_CONNECTING,
	STA_CONNECTED,
	STA_DISCONNECTING,
	STA_DISCONNECTED,
	STA_AUTO_RECONNECT,
	//AP
	AP_CONNECTED,
	AP_DISCONNECTED,
	
}WincState;

typedef struct{
	uint8_t ssid[M2M_MAX_SSID_LEN];
	uint8_t password[M2M_MAX_PSK_LEN];
	tenuM2mSecType SecType;
}ConnInfo,*pConnInfo;

typedef struct{
	SOCKET sock;
	int16_t Bytes;
	uint16_t BytesRemaining;
}SockSndRecv,*pSockSndRecv;

extern QueueHandle_t socket_msg;
extern QueueHandle_t resolve_msg;

extern SemaphoreHandle_t sem_m2m_event;
extern SemaphoreHandle_t sock_recv_event;

extern uint8_t sock_pending_recv;
extern uint8_t sock_pending_send;

extern ConnInfo conn_info;
extern char dns_host[];

int winc_init(void);
int winc_connect(int8_t* ssid,uint8_t SecType,int8_t* password,uint32_t timeoutMS);
int winc_disconnect(uint32_t timeoutMS);
int winc_dns(int8_t *domain, uint32_t *IPAddress);
int winc_mac_address(uint8_t* szmac);
WincState winc_get_state(void);
bool winc_ready_send(void);
int winc_ap_start(char const *ap_ssid);
int winc_ap_stop(void);
int winc_ota(uint8_t* ota_url);
int winc_ping(char const *host,tpfPingCb PingCb);
int winc_rssi(void);
uint32_t winc_get_ip(void);
void winc_fw_version(uint8_t* rev);


#ifdef __cplusplus
}
#endif

#endif /* WINC_H_ */

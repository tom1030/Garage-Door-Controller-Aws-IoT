/*
 * wifi.c
 *
 * Created: 6/6/2017 下午 3:04:16
 *  Author: NSC
 */ 

#include <asf.h>

#include "winc.h"
#include "network_platform.h"

#include "timer_interface.h"

#include "download_fw.h"
#include "periphs.h"

QueueHandle_t wifi_msg = NULL;
QueueHandle_t socket_msg = NULL;
QueueHandle_t resolve_msg = NULL;

SemaphoreHandle_t sem_m2m_event = NULL;
SemaphoreHandle_t sock_recv_event = NULL;

static WincState winc_state = IDLE;
ConnInfo conn_info;

uint8_t sock_pending_recv = 0x00;  //bit mapping that pending recv data need to be handled
uint8_t sock_pending_send = 0x00;	//bit mapping that pending send data need to be handled

static uint32_t winc_ip = 0x00000000;
 
/** host address of DNS Resolution. */
char dns_host[HOSTNAME_MAX_SIZE];

 /***
 *       Wi-Fi notifications callback function.
 ***/
 
 static void wifi_cb(uint8_t u8MsgType, void *pvMsg){
	switch (u8MsgType) {
		case M2M_WIFI_RESP_CON_STATE_CHANGED:{
			tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
			
			if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
				M2M_INFO("Wi-Fi connected,request IP\r\n");
				m2m_wifi_request_dhcp_client();
			} 
			else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
				WincMsg msg;
				if(winc_state == STA_CONNECTING){
					m2m_wifi_connect(conn_info.ssid,m2m_strlen(conn_info.ssid),conn_info.SecType,conn_info.password,M2M_WIFI_CH_ALL);
				}
				else if((winc_state == STA_CONNECTED) || (winc_state == STA_AUTO_RECONNECT)){
					winc_state = STA_AUTO_RECONNECT;
					m2m_wifi_connect(conn_info.ssid,m2m_strlen(conn_info.ssid),conn_info.SecType,conn_info.password,M2M_WIFI_CH_ALL);
				}
				else if(winc_state == STA_DISCONNECTING){
					msg.type = M2M_WIFI_RESP_CON_STATE_CHANGED;
					msg.len = 1;
					msg.msg[0] = STA_DISCONNECTED;
					xQueueSend(wifi_msg, &msg, 0);
				} 
				if(!light_task[LED_R].onoff){

					light_task[LED_R].onoff = true;
					light_task[LED_R].case_bit = 0x15;// 0001_0101
					light_task[LED_R].case_going = 0;
					light_task[LED_R].duration_ms = 5000;
					init_timer(&light_task[LED_R].flash_tmr);
					countdown_ms(&light_task[LED_R].flash_tmr,250);
				}
			}
			
			break;
		}

		case M2M_WIFI_REQ_DHCP_CONF:{
			uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
			WincMsg msg;
			
			M2M_INFO("Wi-Fi IP is %u.%u.%u.%u\r\n",
				pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
			winc_ip = (pu8IPAddress[0] << 24) | (pu8IPAddress[1] << 16) |
				(pu8IPAddress[2] << 8) | (pu8IPAddress[3]);
			
			if(winc_state == STA_CONNECTING){
				msg.type = M2M_WIFI_REQ_DHCP_CONF;
				msg.len = 1;
				msg.msg[0] = STA_CONNECTED;
				xQueueSend(wifi_msg, &msg, 0);
			}
			else if(winc_state == STA_AUTO_RECONNECT)
				winc_state = STA_CONNECTED;
			
			break;
		}
		case M2M_WIFI_RESP_CURRENT_RSSI:{
			sint8	*rssi = (sint8*)pvMsg;
			M2M_INFO("ch rssi %d\n",*rssi);
			break;
		}
		
		default:{
			break;
		}
	}
}

/***
*      Socket application callback function.
***/

static void socket_cb(SOCKET sock, uint8 u8Msg, void * pvMsg)
{
	switch(u8Msg){
		case SOCKET_MSG_BIND:
			if(socket_msg != NULL){
				WincMsg bnd;
				tstrSocketBindMsg* msg = (tstrSocketBindMsg*)(bnd.msg);

				bnd.type = SOCKET_MSG_BIND;
				bnd.len = sizeof(tstrSocketBindMsg);
				msg->status = ((tstrSocketBindMsg*)pvMsg)->status;
				xQueueSend(socket_msg,&bnd,10 / portTICK_PERIOD_MS);
			}
			break;
		
		case SOCKET_MSG_LISTEN:
			if(socket_msg != NULL){
				WincMsg lsn;
				tstrSocketListenMsg* msg = (tstrSocketListenMsg*)(lsn.msg);

				lsn.type = SOCKET_MSG_LISTEN;
				lsn.len = sizeof(tstrSocketListenMsg);
				msg->status = ((tstrSocketListenMsg*)pvMsg)->status;
				xQueueSend(socket_msg,&lsn,10 / portTICK_PERIOD_MS);
			}
			break;

		case SOCKET_MSG_ACCEPT:
			if(socket_msg != NULL){
				WincMsg acpt;
				tstrSocketAcceptMsg* msg = (tstrSocketAcceptMsg*)(acpt.msg);

				acpt.type = SOCKET_MSG_ACCEPT;
				msg->sock = ((tstrSocketAcceptMsg*)pvMsg)->sock;
				acpt.len = sizeof(tstrSocketAcceptMsg);
				xQueueSend(socket_msg,&acpt,10 / portTICK_PERIOD_MS);
			}
			break;

		case SOCKET_MSG_CONNECT:
			if(socket_msg != NULL){
				WincMsg conn;
				tstrSocketConnectMsg* msg = (tstrSocketConnectMsg*)(conn.msg);
				
				conn.type = SOCKET_MSG_CONNECT;
				conn.len = sizeof(tstrSocketConnectMsg);
				msg->sock = ((tstrSocketConnectMsg*)pvMsg)->sock;
				msg->s8Error = ((tstrSocketConnectMsg*)pvMsg)->s8Error;
				xQueueSend(socket_msg,&conn,10 / portTICK_PERIOD_MS);
			}
			break;

		case SOCKET_MSG_RECV:
			if((socket_msg != NULL)&&(sock_recv_event != NULL)){
				WincMsg recv;
				SockSndRecv* SndRecvMsg = (SockSndRecv*)recv.msg;
				tstrSocketRecvMsg* msg = (tstrSocketRecvMsg*)pvMsg;
				if(sock_pending_send){  //if socket is sending, the ignore the recv data. 
					//printf(">>>>>>>>>ignore recv,as send has higher priority\r\n");
					break;
				}
				recv.type = SOCKET_MSG_RECV;
				recv.len = sizeof(SockSndRecv);
		
				SndRecvMsg->sock = sock;
				SndRecvMsg->Bytes = (int16_t)msg->s16BufferSize;
				SndRecvMsg->BytesRemaining = msg->u16RemainingSize;

				sock_pending_recv |= (1 << sock);
				//printf("socket [%d] recv data len %d remaining %d\r\n",sock,msg->s16BufferSize,msg->u16RemainingSize);	
				//wait 10ms until the msg be taken by recv task
				xQueueSend(socket_msg,&recv,10 / portTICK_PERIOD_MS);
				//blocking a while, wait to call recv() to pick the data
				xSemaphoreTake(sock_recv_event,100 / portTICK_PERIOD_MS);

			}
			break;

		case SOCKET_MSG_SEND:
			if(socket_msg != NULL){
				WincMsg snd;
				SockSndRecv* SndRecvMsg = (SockSndRecv*)snd.msg;
				
				snd.type = SOCKET_MSG_SEND;
				snd.len = 2;
			
				SndRecvMsg->sock = sock;
				SndRecvMsg->Bytes = *(int16_t*)(pvMsg); //actul sent length

				sock_pending_send &= ~(1 << sock);
				//printf("socket [%d] send data len %d\r\n",sock,SndRecvMsg->Bytes);
				//wait 10ms until the msg be taken by recv task
				xQueueSend(socket_msg,&snd,10 / portTICK_PERIOD_MS);
			}
			break;

		case SOCKET_MSG_SENDTO:
			
			break;

		case SOCKET_MSG_RECVFROM:
			
			break;
	}
}


/***
*      Callback for the gethostbyname function (DNS Resolution callback).
***/
void resolve_cb(uint8* pu8DomainName, uint32 u32ServerIP)
{
	if(resolve_msg != NULL){
		WincMsg reso;

		reso.type = SOCKET_MSG_DNS_RESOLVE;
		reso.len = 4;
		m2m_memcpy(reso.msg,(uint8*)&u32ServerIP,4);
		strcpy(dns_host,pu8DomainName);
		xQueueSend(resolve_msg,&reso,0);
	}
}

static int _is_ip(const char *host)
{
	uint32_t isv6 = 0;
	char ch;

	while (*host != '\0') {
		ch = *host++;
		if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || ch == ':' || ch == '/') {
			isv6 = 1;
		} else if (ch == '.') {
			if (isv6) {
				return 0;
			}
		} else if ((ch & 0x30) != 0x30) {
			return 0;
		}
	}
	return 1;
}

int winc_init(void){

	tstrWifiInitParam param;
	int8_t ret;
	/* Initialize the BSP. */
	nm_bsp_init();
	/* Initialize Wi-Fi parameters structure. */
	m2m_memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));
	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	param.pfAppMonCb = NULL;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		uint8_t flag;
		M2M_ERR("%s hang-up: m2m_wifi_init call error!(%d)\r\n", __FUNCTION__,ret);
		M2M_ERR("roll-back WINC fw to older one\r\n");

		//If there is any error we will try and rollback
		//m2m_ota_rollback();
	}
		
	/*Initialize socket*/
	socketInit();
	registerSocketCallback(socket_cb,resolve_cb);
	
	wifi_msg = xQueueCreate(1,sizeof(WincMsg));
	socket_msg = xQueueCreate(1,sizeof(WincMsg));
	resolve_msg = xQueueCreate(1,sizeof(WincMsg));
	
	sem_m2m_event = xSemaphoreCreateCounting(1, 0);
	sock_recv_event = xSemaphoreCreateCounting(1, 0);
	
	return 0;
}
/*
** connect the winc to AP
** param : timeoutMS, waiting time in milliseconds to connect to target AP
*/
int winc_connect(int8_t* ssid,uint8_t SecType,int8_t* password,uint32_t timeoutMS){
	int8_t ret;
	WincMsg conn_result;
	
	if((winc_state != IDLE)&&(winc_state != STA_DISCONNECTED)&&(winc_state != AP_DISCONNECTED))
		return -1;
	winc_state = STA_CONNECTING;
	
	M2M_INFO("winc connecting to AP [%s]\r\n",ssid);
	m2m_memset(conn_info.ssid,0,sizeof(conn_info.ssid));
	m2m_memset(conn_info.password,0,sizeof(conn_info.password));
	
	m2m_memcpy(conn_info.ssid,ssid,m2m_strlen(ssid));
	m2m_memcpy(conn_info.password,password,m2m_strlen(password));
	conn_info.SecType = SecType;
	
	ret = m2m_wifi_connect(ssid,m2m_strlen(ssid),SecType,password,M2M_WIFI_CH_ALL);
	if(ret != M2M_SUCCESS)
		return -1;
	if(pdTRUE == xQueuePeek(wifi_msg, &conn_result, timeoutMS / portTICK_PERIOD_MS)){
		if((conn_result.type == M2M_WIFI_REQ_DHCP_CONF) &&
			(conn_result.msg[0] == STA_CONNECTED)){
			xQueueReset(wifi_msg);
			winc_state = STA_CONNECTED;
			
			return 0;  //connect successfully
		}
			
	}
	winc_state = IDLE;
	M2M_ERR("winc connect timeout\r\n");
	//need to wait disconnected message

	
	return -1;  //connect timeout or error
}

/*
** disconnect the winc from connected AP
** param : timeoutMS, waiting time in milliseconds to disconnect from connected AP
*/
int winc_disconnect(uint32_t timeoutMS){
	int8_t ret;
	WincMsg disconn_result;
	
	if(winc_state != STA_CONNECTED)
		return -1;
	
	winc_state = STA_DISCONNECTING;
	
	ret = m2m_wifi_disconnect();
	if(ret != M2M_SUCCESS)
		return -1;
	if(pdTRUE == xQueuePeek(wifi_msg, &disconn_result, timeoutMS / portTICK_PERIOD_MS)){
		if((disconn_result.type == M2M_WIFI_RESP_CON_STATE_CHANGED) &&
			(disconn_result.msg[0] == STA_DISCONNECTED)){
			xQueueReset(wifi_msg);
			winc_state = STA_DISCONNECTED;
			M2M_INFO("winc disconnect from AP\r\n");
			return 0;  //disconnect sucessfully
		}
			
	}
	winc_state = STA_CONNECTED;
	M2M_ERR("winc disconnect timeout\r\n");
	return -1;
}

int winc_dns(int8_t *domain, uint32_t *IPAddress){
	int8_t err;
	WincMsg Msg;

	err = gethostbyname(domain);
	if (err != SOCK_ERR_NO_ERROR){
		M2M_ERR("dns err : %s\r\n", err);
		return -1;
	}
	
	if(pdTRUE == xQueuePeek(resolve_msg, &Msg, 10000 / portTICK_PERIOD_MS)){
		if(Msg.type = SOCKET_MSG_DNS_RESOLVE){
			M2M_INFO("DNS resolve server url : %s\r\n",domain);
			M2M_INFO("Server IP : %u.%u.%u.%u\r\n",
				Msg.msg[0],Msg.msg[1],Msg.msg[2],Msg.msg[3]);
			m2m_memcpy((uint8_t*)IPAddress,Msg.msg,4);
			xQueueReset(resolve_msg);
			return 0;
		}
	}
	return -1;
}

int winc_mac_address(uint8_t* szmac){
	uint8_t mac[6];
	
	if(szmac == NULL)
		return -1;
	m2m_wifi_get_mac_address(mac);
	sprintf( szmac,"%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	M2M_INFO("Mac Address: %02X-%02X-%02X-%02X-%02X-%02X\r\n",\
		mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	return 0;
}

WincState winc_get_state(void){
	return winc_state;
}

int winc_ap_start(char const *ap_ssid){
	tstrM2MAPConfig strM2MAPConfig;
	int8_t ret,len;
	uint8_t winc_mac[12 + 1];
	
	/* Initialize AP mode parameters structure with SSID, channel and OPEN security type. */
	m2m_memset(&strM2MAPConfig, 0x00, sizeof(tstrM2MAPConfig));
	
	winc_mac_address(winc_mac);
	winc_mac[12] = '\0';
	
	len = m2m_strlen(ap_ssid);
	m2m_memcpy(strM2MAPConfig.au8SSID, ap_ssid,len);
	m2m_memcpy(strM2MAPConfig.au8SSID + len,winc_mac + 6,7);
	M2M_INFO("AP SSID [%s]\r\n",(char*)strM2MAPConfig.au8SSID);
	
	strM2MAPConfig.u8ListenChannel = M2M_WIFI_CH_6;
	strM2MAPConfig.u8SecType = M2M_WIFI_SEC_OPEN;
	strM2MAPConfig.au8DHCPServerIP[0] = 192;
	strM2MAPConfig.au8DHCPServerIP[1] = 168;
	strM2MAPConfig.au8DHCPServerIP[2] = 1;
	strM2MAPConfig.au8DHCPServerIP[3] = 1;

	/* Bring up AP mode with parameters structure. */
	ret = m2m_wifi_enable_ap(&strM2MAPConfig);
	if (M2M_SUCCESS != ret) {
		M2M_ERR("%s: m2m_wifi_enable_ap call error!\r\n",__func__);
		return -1;
	}
	winc_state = AP_CONNECTED;
	return 0;
}

int winc_ap_stop(void){
	int8_t ret;
	ret = m2m_wifi_disable_ap();
	if (M2M_SUCCESS != ret) {
		M2M_ERR("%s: m2m_wifi_disable_ap call error!\r\n",__func__);
		return -1;
	}
	winc_state = AP_DISCONNECTED;
	return 0;
}

/**
 * \brief Callback to get the OTA update event.
 *
 * \param[in] u8OtaUpdateStatusType type of OTA update status notification. Possible types are:
 * - [DL_STATUS](@ref DL_STATUS)
 * - [SW_STATUS](@ref SW_STATUS)
 * - [RB_STATUS](@ref RB_STATUS)
 * \param[in] u8OtaUpdateStatus type of OTA update status detail. Possible types are:
 * - [OTA_STATUS_SUCSESS](@ref OTA_STATUS_SUCSESS)
 * - [OTA_STATUS_FAIL](@ref OTA_STATUS_FAIL)
 * - [OTA_STATUS_INVAILD_ARG](@ref OTA_STATUS_INVAILD_ARG)
 * - [OTA_STATUS_INVAILD_RB_IMAGE](@ref OTA_STATUS_INVAILD_RB_IMAGE)
 * - [OTA_STATUS_INVAILD_FLASH_SIZE](@ref OTA_STATUS_INVAILD_FLASH_SIZE)
 * - [OTA_STATUS_AlREADY_ENABLED](@ref OTA_STATUS_AlREADY_ENABLED)
 * - [OTA_STATUS_UPDATE_INPROGRESS](@ref OTA_STATUS_UPDATE_INPROGRESS)
 */
static void OtaUpdateCb(uint8_t u8OtaUpdateStatusType, uint8_t u8OtaUpdateStatus)
{
	printf("OtaUpdateCb %d %d\r\n", u8OtaUpdateStatusType, u8OtaUpdateStatus);
	if (u8OtaUpdateStatusType == DL_STATUS) {
		if (u8OtaUpdateStatus == OTA_STATUS_SUCSESS) {
			M2M_INFO("OtaUpdateCb m2m_ota_switch_firmware start.\r\n");
			m2m_ota_switch_firmware();
		} else {
			M2M_INFO("OtaUpdateCb FAIL u8OtaUpdateStatus %d\r\n", u8OtaUpdateStatus);
		}
	} else if (u8OtaUpdateStatusType == SW_STATUS) {
		if (u8OtaUpdateStatus == OTA_STATUS_SUCSESS) {
			M2M_INFO("WINC switch to OTA upgraded firmwre.\r\n");
			update_ota_record(true);
			system_reset();
		}
	} else if (u8OtaUpdateStatusType == RB_STATUS)
	{
		if (u8OtaUpdateStatus == OTA_STATUS_SUCSESS) {
			M2M_INFO("WINC switch to roll-back firmwre.\r\n");
			update_ota_record(false);
			system_reset();
		}
	}
}

/**
 * \brief OTA notify callback.
 *
 * OTA notify callback typedef.
 */
static void OtaNotifCb(tstrOtaUpdateInfo *pv)
{
	M2M_INFO("OtaNotifCb \r\n");
}

int winc_ota(uint8_t* ota_url){
	int i = 0;
	
	m2m_ota_init(OtaUpdateCb, OtaNotifCb);
	
	M2M_INFO("WINC start OTA %s\r\n",ota_url);
	/* Start OTA Firmware download. */
	//only support http://
	m2m_ota_start_update(ota_url);
	//Wait forever and switch task
	vTaskDelay(portMAX_DELAY);
}

int winc_ping(char const *host,tpfPingCb PingCb){
	uint32_t u32IP;
	
	if(_is_ip(host)){
		u32IP = nmi_inet_addr(host);
	}
	else {
		winc_dns(host,&u32IP);
	}
	M2M_INFO("Ping %s\r\n",host);
	m2m_ping_req(u32IP,0,PingCb);
}

int winc_rssi(void){
	m2m_wifi_req_curr_rssi();
}

uint32_t winc_get_ip(void){
	return winc_ip;
}

void winc_fw_version(uint8_t* rev){
	tstrM2mRev fwRev;

	if(sizeof(rev) < 3)
		return;
	m2m_wifi_get_firmware_version(&fwRev);
	rev[0] = fwRev.u8FirmwareMajor;
	rev[1] = fwRev.u8FirmwareMinor;
	rev[2] = fwRev.u8FirmwarePatch;
}



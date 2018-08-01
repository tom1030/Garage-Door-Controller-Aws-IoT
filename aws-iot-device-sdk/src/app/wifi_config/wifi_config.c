/*
 * wifi_config.c
 *
 * Created: 25/7/2017 上午 11:30:05
 *  Author: NSC
 */
 
#include <asf.h>
 
#include "wifi_config.h"
#include "timer_interface.h"
#include "winc.h"
#include "jsmn.h"
#include "nv_eeprom.h"

#define SSID_TOKEN_STRING "ssid"
#define PASSWORD_TOKEN_STRING "password"

static jsmn_parser JsonParser;
static jsmntok_t TokenStruct[10];
static int32_t tokenCount;

#define CONFIG_FLASH_INTERVAL    500
//wait configure message for 10 minutes
//500ms * 1200 == 600s
#define CONFIG_TIMEOUT    1200

int _socket_bind(SOCKET sock,uint16_t port){
	struct sockaddr_in addr;
	WincMsg Msg;
	int ret = -1;

	addr.sin_family = AF_INET;
	addr.sin_port = _htons(port);
	addr.sin_addr.s_addr = 0;

	if(SOCK_ERR_NO_ERROR != bind(sock,(struct sockaddr*)&addr,sizeof(addr)))
		return -1;
	if(pdTRUE == xQueuePeek(socket_msg, &Msg, 100 / portTICK_PERIOD_MS)){
		if(Msg.type == SOCKET_MSG_BIND){
			tstrSocketBindMsg* bnd_msg = (tstrSocketBindMsg*)Msg.msg;
			xQueueReset(socket_msg);
			ret = bnd_msg->status;
		}
	}
	return ret;
}

int _socket_listen(SOCKET sock){
	WincMsg Msg;
	int ret = -1;
	
	if(SOCK_ERR_NO_ERROR != listen(sock,0))
		return -1;
	
	if(pdTRUE == xQueuePeek(socket_msg, &Msg, 100 / portTICK_PERIOD_MS)){
		if(Msg.type == SOCKET_MSG_LISTEN){
			tstrSocketListenMsg* lsn_msg = (tstrSocketListenMsg*)Msg.msg;
			xQueueReset(socket_msg);
			ret = lsn_msg->status;
		}
	}
	return ret;
}

int _socket_accept(SOCKET sock){
	WincMsg Msg;
	int ret = -1;
	
	if(SOCK_ERR_NO_ERROR != accept(sock,0,0))
		return -1;
	
	if(pdTRUE == xQueuePeek(socket_msg, &Msg, 100 / portTICK_PERIOD_MS)){
		if(Msg.type == SOCKET_MSG_ACCEPT){
			tstrSocketAcceptMsg* acpt_msg = (tstrSocketAcceptMsg*)Msg.msg;
			xQueueReset(socket_msg);
			ret = acpt_msg->sock;
		}
	}
	return ret;
}

int _socket_recv(SOCKET sock,uint8_t* buff,uint16_t len){
	WincMsg Msg;
	int16_t ret = 0;
	
	if((buff == NULL)||(SOCK_ERR_NO_ERROR != recv(sock,buff,len,0)))
		return -1;
	
	if(pdTRUE == xQueuePeek(socket_msg, &Msg, 10 / portTICK_PERIOD_MS)){
		SockSndRecv* recv_msg = (SockSndRecv*)Msg.msg;
		if((Msg.type == SOCKET_MSG_RECV)&&(recv_msg->sock == sock)){
			xQueueReset(socket_msg);
			ret = recv_msg->Bytes;
			sock_pending_recv &= ~(1 << sock);
			xSemaphoreGive(sock_recv_event);
		}
	}	
	return ret;
}

int8_t _jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if(tok->type == JSMN_STRING) {
		if((int) strlen(s) == tok->end - tok->start) {
			if(strncmp(json + tok->start, s, (size_t) (tok->end - tok->start)) == 0) {
				return 0;
			}
		}
	}
	return -1;
}


bool _isJsonValidAndParse(const char *pJsonDocument) {
	
	jsmn_init(&JsonParser);
	tokenCount = jsmn_parse(&JsonParser, pJsonDocument, strlen(pJsonDocument), TokenStruct,
							sizeof(TokenStruct) / sizeof(TokenStruct[0]));

	if(tokenCount < 0) {
		printf("%s Failed to parse JSON: %d\n", __func__,tokenCount);
		return false;
	}

	/* Assume the top-level element is an object */
	if(tokenCount < 1 || TokenStruct[0].type != JSMN_OBJECT) {
		printf("%s Top Level is not an object\n",__func__);
		return false;
	}
	
	return true;
}

bool _extractTokenString(const char *pJsonDocument, char* token, char *pExtractedToken) {
	int32_t i;
	uint8_t length;
	jsmntok_t JsonToken;

	for(i = 1; i < tokenCount; i++) {
		if(_jsoneq(pJsonDocument, &TokenStruct[i], token) == 0) {
			JsonToken = TokenStruct[i + 1];
			length = (uint8_t) (JsonToken.end - JsonToken.start);
			strncpy(pExtractedToken, pJsonDocument + JsonToken.start, length);
			pExtractedToken[length] = '\0';
			return true;
		}
	}
	
	return false;
}

int do_wifi_config(void){
	int ret = 0;
	Timer flash_tmr,udp_tmr;
	SOCKET tcp_server = -1;
	SOCKET tcp_client = -1;
	SOCKET udp_bd = -1;
	uint8_t RecvBuff[200 + 1]; 
	uint8_t SSID[M2M_MAX_SSID_LEN];
	uint8_t password[M2M_MAX_PSK_LEN];
	uint16_t config_tmr = 0;
			
	if(STA_CONNECTED == winc_get_state())
		ret = winc_disconnect(10000);  //10 sec timeout
	
	if(ret < 0 )
		return -1;
	
	if(winc_ap_start("GDC_AP_") < 0)
		return -2;
	
	tcp_server = socket(AF_INET, SOCK_STREAM, /*IPPROTO_TCP*/0);
	if(tcp_server < 0){
		return -3;
	}
	
	if(_socket_bind(tcp_server,80) < 0)
		return -4;
	if(_socket_listen(tcp_server) < 0)
		return -5;
	
	init_timer(&flash_tmr);
	countdown_ms(&flash_tmr,CONFIG_FLASH_INTERVAL);

	while(config_tmr < CONFIG_TIMEOUT){
		//flash indication
		if(has_timer_expired(&flash_tmr)){
			LED_Toggle(LED_G_PIN);
			init_timer(&flash_tmr);
			countdown_ms(&flash_tmr,CONFIG_FLASH_INTERVAL);
			config_tmr++;
		}
		if(tcp_client < 0){
			tcp_client = _socket_accept(tcp_server);
			if(tcp_client < 0)
				continue;
		}
		ret = _socket_recv(tcp_client,RecvBuff,200);
		if(ret < 0){
			socket_close(tcp_client);
			tcp_client = -1;
		}
		else if((ret > 0)&&(ret < 200)){
			RecvBuff[ret] = '\0';    // jsmn_parse relies on a string
			if(!_isJsonValidAndParse(RecvBuff))
				continue;
			if(!_extractTokenString(RecvBuff,SSID_TOKEN_STRING,SSID))
				continue;
			if(!_extractTokenString(RecvBuff,PASSWORD_TOKEN_STRING,password))
				continue;
			break;
		}
	}
	
	LED_Off(LED_G_PIN);

	socket_close(tcp_server);
	winc_ap_stop();
	vTaskDelay(10 / portTICK_PERIOD_MS);  //wait the ap close
	if(config_tmr >= CONFIG_TIMEOUT){
		memcpy(SSID,conn_info.ssid,M2M_MAX_SSID_LEN);
		memcpy(password,conn_info.password,M2M_MAX_PSK_LEN);
		
		//if do wifi config timeout, re-try to connect to old AP
		ret = winc_connect(SSID,conn_info.SecType,password,60000);
	} else {
		ret = winc_connect(SSID,M2M_WIFI_SEC_WPA_PSK,password,60000);
		if(ret == 0){
			struct sockaddr_in addr;
			uint8_t buff[100] = "";
			uint8_t snPrintfReturn;

			write_to_control_block(WIFI_CONFIG_OFFSET,(uint8_t*)&conn_info,sizeof(conn_info));
			//add UDP broadcasting beacon here to notify the app that connecting successfully
			udp_bd = socket(AF_INET, SOCK_DGRAM, /*IPPROTO_TCP*/0);
			if(udp_bd < 0)
				return -6;

			/* Initialize socket address structure. */
			addr.sin_family = AF_INET;
			addr.sin_port = _htons(6666);
			addr.sin_addr.s_addr = _htonl(0xFFFFFFFF);

			snPrintfReturn = snprintf(buff,100, "{\"wifi_config\":{\"result\":true}}");
			if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= 100)) {
				return -7;
			}

			init_timer(&udp_tmr);
			countdown_sec(&udp_tmr,30);   // 30 secs
			while(!has_timer_expired(&udp_tmr)){
				sendto(udp_bd,(void*)buff,snPrintfReturn,0,&addr,sizeof(addr));
			}
		}
		
	}
	
	return ret;
}

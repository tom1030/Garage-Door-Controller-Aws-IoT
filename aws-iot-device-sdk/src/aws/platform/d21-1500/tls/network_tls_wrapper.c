/*
 * network_tls_wrapper.c
 *
 * Created: 2/6/2017 下午 4:11:03
 *  Author: NSC
 */ 

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <string.h>
#include <timer_platform.h>
#include <network_interface.h>

#include "aws_iot_error.h"
#include "aws_iot_log.h"
#include "network_interface.h"
#include "network_platform.h"
#include "winc.h"
#include "timer_interface.h"

/* This is the value used for ssl read timeout */
#define IOT_SSL_READ_TIMEOUT 10

BuffDesc recv_buff;

void _iot_tls_set_connect_params(Network *pNetwork, char *pRootCALocation, char *pDeviceCertLocation,
								 char *pDevicePrivateKeyLocation, char *pDestinationURL,
								 uint16_t destinationPort, uint32_t timeout_ms, bool ServerVerificationFlag) {
	pNetwork->tlsConnectParams.DestinationPort = destinationPort;
	pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
	pNetwork->tlsConnectParams.pDeviceCertLocation = pDeviceCertLocation;
	pNetwork->tlsConnectParams.pDevicePrivateKeyLocation = pDevicePrivateKeyLocation;
	pNetwork->tlsConnectParams.pRootCALocation = pRootCALocation;
	pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
	pNetwork->tlsConnectParams.ServerVerificationFlag = ServerVerificationFlag;
}

IoT_Error_t iot_tls_init(Network *pNetwork, char *pRootCALocation, char *pDeviceCertLocation,
						 char *pDevicePrivateKeyLocation, char *pDestinationURL,
						 uint16_t destinationPort, uint32_t timeout_ms, bool ServerVerificationFlag) {

	_iot_tls_set_connect_params(pNetwork, pRootCALocation, pDeviceCertLocation, pDevicePrivateKeyLocation,
								pDestinationURL, destinationPort, timeout_ms, ServerVerificationFlag);

	pNetwork->connect = iot_tls_connect;
	pNetwork->read = iot_tls_read;
	pNetwork->write = iot_tls_write;
	pNetwork->disconnect = iot_tls_disconnect;
	pNetwork->isConnected = iot_tls_is_connected;
	pNetwork->destroy = iot_tls_destroy;

	return SUCCESS;
}

static bool PingFlag = 0;
static uint8_t PingCode;
static uint32_t pingIP;

void _ping_callback(uint32 u32IPAddr, uint32 u32RTT, uint8 u8ErrorCode){
	PingCode = u8ErrorCode;
	pingIP = u32IPAddr;
	PingFlag = 1;
}

IoT_Error_t iot_tls_is_connected(Network *pNetwork) {
	/* Use this to add implementation which can 
	check for physical layer disconnect */
#if 0
	TLSDataParams* tlsDataParams = &(pNetwork->tlsDataParams);
	Timer tmr;
	uint32_t ServerIP;
	PingFlag = 0;
	ServerIP = tlsDataParams->server_fd.sin_addr.s_addr;
	m2m_ping_req(ServerIP,0,(tpfPingCb)_ping_callback);
	init_timer(&tmr);
	countdown_ms(&tmr,60000);  // 60s ping timeout
	while((!has_timer_expired(&tmr))&&(!PingFlag));
	if((PingFlag)&&(PingCode == PING_ERR_SUCCESS)&&(pingIP == ServerIP))
		return NETWORK_PHYSICAL_LAYER_CONNECTED;
	return NETWORK_PHYSICAL_LAYER_DISCONNECTED;
#else
	WincState state;
	state = winc_get_state();
	if(state == STA_CONNECTED)
		return NETWORK_PHYSICAL_LAYER_CONNECTED;
	else
		return NETWORK_PHYSICAL_LAYER_DISCONNECTED;
#endif

}

IoT_Error_t iot_tls_connect(Network *pNetwork, TLSConnectParams *params) {
	int ret = 0;
	TLSConnectParams* tlsConnectParams = NULL;
	TLSDataParams* tlsDataParams = NULL;
	WincMsg conn_msg;
	Timer tmr;
	uint32_t ServerIP;

	
	if(NULL == pNetwork) {
		return NULL_VALUE_ERROR;
	}

	if(NULL != params) {
		_iot_tls_set_connect_params(pNetwork, params->pRootCALocation, params->pDeviceCertLocation,
									params->pDevicePrivateKeyLocation, params->pDestinationURL,
									params->DestinationPort, params->timeout_ms, params->ServerVerificationFlag);
	}

	tlsConnectParams = &(pNetwork->tlsConnectParams);
	tlsDataParams = &(pNetwork->tlsDataParams);
	
	if(tlsDataParams->ssl >= 0){  //when reconnect need close socket first
		iot_tls_disconnect(pNetwork);
		iot_tls_destroy(pNetwork);
	}
		
	if((tlsDataParams->ssl = socket(AF_INET, SOCK_STREAM,SOCKET_FLAGS_SSL))<0){
		IOT_ERROR("ssl socket init fail");
		return NETWORK_ERR_NET_SOCKET_FAILED;
	}

	if(winc_dns(tlsConnectParams->pDestinationURL,&ServerIP)<0){
		IOT_ERROR("Server IP resolve fail");
		return NETWORK_ERR_NET_UNKNOWN_HOST;
	}

	if(ServerIP == 0)
		return NETWORK_ERR_NET_UNKNOWN_HOST;

	tlsDataParams->server_fd.sin_family = AF_INET;
	tlsDataParams->server_fd.sin_port = _htons(tlsConnectParams->DestinationPort);
	tlsDataParams->server_fd.sin_addr.s_addr = ServerIP;
	ret = connect(tlsDataParams->ssl,(struct sockaddr*)(&(tlsDataParams->server_fd)),sizeof( struct sockaddr_in));
	if(ret != SOCK_ERR_NO_ERROR){
		IOT_ERROR("ssl socket connect fail");
		return NETWORK_ERR_NET_CONNECT_FAILED;
	}
	init_timer(&tmr);
	countdown_ms(&tmr,pNetwork->tlsConnectParams.timeout_ms);
	while(!has_timer_expired(&tmr)){
		if(pdTRUE == xQueuePeek(socket_msg, &conn_msg, pNetwork->tlsConnectParams.timeout_ms / portTICK_PERIOD_MS)){
			if(conn_msg.type == SOCKET_MSG_CONNECT){
				tstrSocketConnectMsg* msg = (tstrSocketConnectMsg*)(conn_msg.msg);
				if(tlsDataParams->ssl == msg->sock){
					xQueueReset(socket_msg);
					if(0 == msg->s8Error){
						return SUCCESS;
					}
					else{
						IOT_ERROR("ssl socket connect error %d",msg->s8Error);
						return FAILURE;
					}
				}
			}
		}
	}
	
	IOT_ERROR("ssl socket connect timeout fail");
	return NETWORK_SSL_CONNECT_TIMEOUT_ERROR;
}

IoT_Error_t iot_tls_write(Network *pNetwork, unsigned char *pMsg,size_t len, Timer *timer, size_t *written_len) {
	TLSDataParams* tlsDataParams = &(pNetwork->tlsDataParams);
	uint16_t written_so_far = 0;
	int ret;
	WincMsg send_msg;

	//printf("%s len %d\r\n",__func__,len);
	for(written_so_far = 0;
		written_so_far < len && !has_timer_expired(timer); written_so_far += ret){

		sock_pending_send |= (1 << tlsDataParams->ssl);
		if(sock_pending_recv){
			//printf("********ignore recv,as write has higher priority\r\n");
			sock_pending_recv &= ~(1 << tlsDataParams->ssl);
			xQueueReset(socket_msg);
			xSemaphoreGive(sock_recv_event);
		}
		//socket send
		ret = send(tlsDataParams->ssl,pMsg + written_so_far,len - written_so_far,0);
		if(ret != SOCK_ERR_NO_ERROR){
			//send fail
			IOT_ERROR("ssl socket write fail");
			return NETWORK_SSL_WRITE_ERROR;
		}
		ret = 0;
		while(!has_timer_expired(timer)){
			if(pdTRUE == xQueuePeek(socket_msg, &send_msg, left_ms(timer)/portTICK_PERIOD_MS)){
				SockSndRecv* SndRecvmsg = (SockSndRecv*)send_msg.msg;
				if((send_msg.type == SOCKET_MSG_SEND)&&(tlsDataParams->ssl == SndRecvmsg->sock)){
					//send successfully
					ret = SndRecvmsg->Bytes;
					xQueueReset(socket_msg);
					break;
				}
				//peek out other type or socket's msg and need continue to peek until timer expire 
			}
			//peek timeout
			
		}
		//continue to send the rest length(len - ret) data 
		//or timer expired
	}
	*written_len = written_so_far;
	if(written_so_far == len)
		return SUCCESS;
	else if(has_timer_expired(timer)){
		IOT_ERROR("ssl socket write timeout");
		return NETWORK_SSL_WRITE_TIMEOUT_ERROR;
	}
}

IoT_Error_t iot_tls_read(Network *pNetwork, unsigned char *pMsg, size_t len, Timer *timer, size_t *read_len) {
	TLSDataParams* tlsDataParams = &(pNetwork->tlsDataParams);
	size_t rxLen = 0;
	int ret;
	WincMsg recv_msg;
	
	while (len > 0) {
		//check the buffer first, if there no pending then call recv()
		if(recv_buff.size_used > 0){
			//read data from buffer
			ret = recv_buff.size_used > len ? len : recv_buff.size_used;
			m2m_memcpy(pMsg,recv_buff.data + recv_buff.pick_offset,ret);
			recv_buff.pick_offset += ret;
			recv_buff.size_used -= ret;

			rxLen += ret;
			pMsg += ret;
			len -= ret;
			if(0 == recv_buff.size_used){ // buffer is empty
				sock_pending_recv &= ~(1 << tlsDataParams->ssl);
				xSemaphoreGive(sock_recv_event);
			}
		}
		else{
			ret = recv(tlsDataParams->ssl, recv_buff.data, BUFF_SIZE, 0);
			if(ret != SOCK_ERR_NO_ERROR){
				//call receive fail
				IOT_ERROR("ssl socket read fail");
				return NETWORK_SSL_READ_ERROR;
			}
			// This read will timeout after IOT_SSL_READ_TIMEOUT if there's no data to be read
			if(pdTRUE == xQueuePeek(socket_msg, &recv_msg, IOT_SSL_READ_TIMEOUT/portTICK_PERIOD_MS)){
				SockSndRecv* SndRecvmsg = (SockSndRecv*)recv_msg.msg;
				if((recv_msg.type == SOCKET_MSG_RECV)&&(tlsDataParams->ssl == SndRecvmsg->sock)){
					ret = SndRecvmsg->Bytes;
					xQueueReset(socket_msg);
					if(ret > 0){
						recv_buff.size_used = ret;
						recv_buff.pick_offset = 0;
					}
					else{ 
						//negative or zero for receive error
						if(ret < 0)
							IOT_ERROR("ssl socket read error %d",ret);
						return NETWORK_SSL_READ_ERROR;
					}
				}
				else{
					//read other type or socket data
				}
			}
		}
		// Evaluate timeout after the read to make sure read is done at least once
		if (has_timer_expired(timer)) {
			break;
		}
	}

	if (len == 0) {
		*read_len = rxLen;
		return SUCCESS;
	}

	if (rxLen == 0) {
		return NETWORK_SSL_NOTHING_TO_READ;
	} else {
		IOT_ERROR("ssl socket read timeout");
		return NETWORK_SSL_READ_TIMEOUT_ERROR;
	}
}

IoT_Error_t iot_tls_disconnect(Network *pNetwork) {
	TLSDataParams* tlsDataParams = &(pNetwork->tlsDataParams);
	
	socket_close(tlsDataParams->ssl);
	return SUCCESS;
}

IoT_Error_t iot_tls_destroy(Network *pNetwork) {
	TLSDataParams* tlsDataParams = &(pNetwork->tlsDataParams);
	//cleanup when close socket
	if((sock_pending_recv & (1 << tlsDataParams->ssl)) ||
		(sock_pending_send & (1 << tlsDataParams->ssl))){
		sock_pending_recv &= ~(1 << tlsDataParams->ssl);
		sock_pending_send &= ~(1 << tlsDataParams->ssl);
	}
	xQueueReset(socket_msg);
	xQueueReset(resolve_msg);
	tlsDataParams->ssl = -1;
	return SUCCESS;
}

#ifdef __cplusplus
}
#endif


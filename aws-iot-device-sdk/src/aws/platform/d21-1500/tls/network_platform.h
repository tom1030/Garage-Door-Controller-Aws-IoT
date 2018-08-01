/*
 * network_tls_wrapper.h
 *
 * Created: 2/6/2017 下午 4:11:12
 *  Author: NSC
 */ 


#ifndef NETWORK_TLS_WRAPPER_H_
#define NETWORK_TLS_WRAPPER_H_

#include <asf.h>
#include "socket/source/socket_internal.h"

#ifdef __cplusplus
extern "C" {
#endif



#define BUFF_SIZE (128)


/**
 * @brief TLS Connection Parameters
 *
 * Defines a type containing TLS specific parameters to be passed down to the
 * TLS networking layer to create a TLS secured socket.
 */
typedef struct _TLSDataParams {
	//mbedtls_entropy_context entropy;
	//mbedtls_ctr_drbg_context ctr_drbg;
	//mbedtls_ssl_context ssl;
	//mbedtls_ssl_config conf;
	//uint32_t flags;
	//mbedtls_x509_crt cacert;
	//mbedtls_x509_crt clicert;
	//mbedtls_pk_context pkey;
	//mbedtls_net_context server_fd;
	SOCKET ssl;
	struct sockaddr_in server_fd;
	
}TLSDataParams;

typedef struct{
	uint16_t size_used;
	uint16_t pick_offset;
	uint8_t data[BUFF_SIZE];
}BuffDesc;

extern BuffDesc recv_buff;

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_TLS_WRAPPER_H_ */
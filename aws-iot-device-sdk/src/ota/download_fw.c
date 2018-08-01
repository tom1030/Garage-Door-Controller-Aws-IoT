/*
 * ota.c
 *
 * Created: 21/8/2017 下午 2:52:57
 *  Author: NSC
 */ 


#include <asf.h>
#include <errno.h>

#include "download_fw.h"
#include "http_client.h"
#include "winc.h"
#include "nv_eeprom.h"
#include "app.h"

#include "driver/include/m2m_wifi.h"
#include "driver/include/m2m_ota.h"
#include "socket/include/socket.h"

#ifdef SD_MMC_ENABLE
#include "sd_mmc.h"
#include "sd_mmc_mem.h"
#include "sd_mmc_spi.h"
#endif
#ifdef SST25VF_ENABLE
#include "conf_sst25vf.h"
#endif

#include "dfu_serial.h"

/** File download processing state. */
static download_state down_state = NOT_READY;
/** SD/MMC mount */
static FATFS fatfs;
/** File pointer for file download. */
static FIL file_object;
/** Http content length. */
static uint32_t http_file_size = 0;
/** Receiving content length. */
static uint32_t received_file_size = 0;

/** Instance of Timer module. */
struct sw_timer_module swt_module_inst;

/** Instance of HTTP client module. */
struct http_client_module http_client_module_inst;

/** Server host name. */
static char* fw_download_url = NULL;
static char* fw_download_file = NULL;

/** Download file name. */
char save_file_name[MAIN_MAX_FILE_NAME_LENGTH + 1] = "0:";

fw_ota_record_t fw_ota;

#define MAIN_UNIT_FW_NAME    "main-unit.bin"
#define MAIN_UNIT_FW_NAME_RB    "main-unit-roll-back.bin"
#define BLE_INIT_PACKET    "init-packet.dat"
#define BLE_FW_IMAGE    "firmware-image.bin"

#ifdef SST25VF_ENABLE
static struct spi_module sst25vf_spi;
struct sst25vf_chip_module sst25vf_chip;
#endif
/**
 * \brief Initialize to download processing state.
 */
static void init_state(void)
{
	down_state = NOT_READY;
}

/**
 * \brief Clear state parameter at download processing state.
 * \param[in] mask Check download_state.
 */
static void clear_state(download_state mask)
{
	down_state &= ~mask;
}

/**
 * \brief Add state parameter at download processing state.
 * \param[in] mask Check download_state.
 */
static void add_state(download_state mask)
{
	down_state |= mask;
}

/**
 * \brief File download processing state check.
 * \param[in] mask Check download_state.
 * \return true if this state is set, false otherwise.
 */

static inline bool is_state_set(download_state mask)
{
	return ((down_state & mask) != 0);
}

/**
 * \brief File existing check.
 * \param[in] fp The file pointer to check.
 * \param[in] file_path_name The file name to check.
 * \return true if this file name is exist, false otherwise.
 */
static bool is_exist_file(FIL *fp, const char *file_path_name)
{
	if (fp == NULL || file_path_name == NULL) {
		return false;
	}

	FRESULT ret = f_open(&file_object, (char const *)file_path_name, FA_OPEN_EXISTING);
	f_close(&file_object);
	if(FR_NO_FILESYSTEM == ret){ //not exist
		ret = f_mkfs(0,1,0);  // SFD
		if(ret != FR_OK){
			printf("File system make error! ret:%d\r\n", ret);
		}
		printf("Mount data Flash...\r\n");
		memset(&fatfs, 0, sizeof(FATFS));
		ret = f_mount(LUN_ID_SST25VF_MEM, &fatfs);
		if (FR_INVALID_DRIVE == ret) {
			printf("data Flash Mount failed. res %d\r\n", ret);
		}
		return false;
	}
	return (ret == FR_OK);
}

/**
 * \brief Make to unique file name.
 * \param[in] fp The file pointer to check.
 * \param[out] file_path_name The file name change to uniquely and changed name is returned to this buffer.
 * \param[in] max_len Maximum file name length.
 * \return true if this file name is unique, false otherwise.
 */
static bool rename_to_unique(FIL *fp, char *file_path_name, uint8_t max_len)
{
	#define NUMBRING_MAX (3)
	#define ADDITION_SIZE (NUMBRING_MAX + 1) /* '-' character is added before the number. */
	uint16_t i = 1, name_len = 0, ext_len = 0, count = 0;
	char name[MAIN_MAX_FILE_NAME_LENGTH + 1] = {0};
	char ext[MAIN_MAX_FILE_EXT_LENGTH + 1] = {0};
	char numbering[NUMBRING_MAX + 1] = {0};
	char *p = NULL;
	bool valid_ext = false;
	if (file_path_name == NULL) {
		return false;
	}

	if (!is_exist_file(fp, file_path_name)) {
		return true;
	} else if (strlen(file_path_name) > MAIN_MAX_FILE_NAME_LENGTH) {
		return false;
	}

	p = strrchr(file_path_name, '.');
	if (p != NULL) {
		ext_len = strlen(p);
		if (ext_len < MAIN_MAX_FILE_EXT_LENGTH) {
			valid_ext = true;
			strcpy(ext, p);
			if (strlen(file_path_name) - ext_len > MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE) {
				name_len = MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE - ext_len;
				strncpy(name, file_path_name, name_len);
			} else {
				name_len = (p - file_path_name);
				strncpy(name, file_path_name, name_len);
			}
		} else {
			name_len = MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE;
			strncpy(name, file_path_name, name_len);
		}
	} else {
		name_len = MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE;
		strncpy(name, file_path_name, name_len);
	}

	name[name_len++] = '-';

	for (i = 0, count = 1; i < NUMBRING_MAX; i++) {
		count *= 10;
	}
	for (i = 1; i < count; i++) {
		sprintf(numbering, MAIN_ZERO_FMT(NUMBRING_MAX), i);
		strncpy(&name[name_len], numbering, NUMBRING_MAX);
		if (valid_ext) {
			strcpy(&name[name_len + NUMBRING_MAX], ext);
		}

		if (!is_exist_file(fp, name)) {
			memset(file_path_name, 0, max_len);
			strcpy(file_path_name, name);
			return true;
		}
	}
	return false;
}

/**
 * \brief Start file downloading via http connection.
 */
static void start_download(void)
{
	if (!is_state_set(STORAGE_READY)) {
		printf("start_download: MMC storage not ready.\r\n");
		return;
	}

	if (!is_state_set(WIFI_CONNECTED)) {
		printf("start_download: Wi-Fi is not connected.\r\n");
		return;
	}

	if (is_state_set(GET_REQUESTED)) {
		printf("start_download: request is sent already.\r\n");
		return;
	}

	if (is_state_set(DOWNLOADING)) {
		printf("start_download: running download already.\r\n");
		return;
	}

	/* Send the HTTP request. */
	printf("start_download: send http request.\r\n");
	if(fw_download_url != NULL)
		http_client_send_request(&http_client_module_inst, fw_download_url, HTTP_METHOD_GET, NULL, NULL);
}

/**
 * \brief Store received packet to file.
 * \param[in] data Packet data.
 * \param[in] length Packet data length.
 */
static void store_file_packet(char *data, uint32_t length)
{
	FRESULT ret;
	
	if ((data == NULL) || (length < 1)) {
		printf("store_file_packet: Empty data.\r\n");
		return;
	}

	if (!is_state_set(DOWNLOADING)) {
		char *cp = NULL;
		
	#ifdef SD_MMC_ENABLE
		save_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	#endif
	#ifdef SST25VF_ENABLE
		save_file_name[0] = LUN_ID_SST25VF_MEM + '0';
	#endif
		save_file_name[1] = ':';
		cp = (char *)(fw_download_url + strlen(fw_download_url));
		while (*cp != '/') {
			cp--;
		}
		if (strlen(cp) > 1) {
			cp++;
			strcpy(&save_file_name[2], cp);
		} else {
			printf("store_file_packet: File name is invalid. Download canceled.\r\n");
			add_state(CANCELED);
			return;
		}

		rename_to_unique(&file_object, save_file_name, MAIN_MAX_FILE_NAME_LENGTH);
		printf("store_file_packet: Creating to file :%s\r\n", save_file_name);
		ret = f_open(&file_object, (char const *)save_file_name, FA_CREATE_ALWAYS | FA_WRITE);
		if (ret != FR_OK) {
			printf("store_file_packet: File create error! ret:%d\r\n", ret);
			return;
		}

		received_file_size = 0;
		add_state(DOWNLOADING);
	}

	if (data != NULL) {
		UINT wsize = 0;
		ret = f_write(&file_object, (const void *)data, length, &wsize);
		if (ret != FR_OK) {
			f_close(&file_object);
			add_state(CANCELED);
			printf("store_file_packet: File write error. Download canceled.\r\n");
			return;
		}

		received_file_size += wsize;
		printf("store_file_packet: received[%lu], file size[%lu]\r\n", (unsigned long)received_file_size, (unsigned long)http_file_size);
		if (received_file_size >= http_file_size) {
			f_close(&file_object);
			printf("store_file_packet: Download completed. location:[%s]\r\n", save_file_name);

			if(((!strcmp(fw_ota.target,"BLE Central")) || (!strcmp(fw_ota.target,"Door Sensor")))
				&&(!strcmp(fw_download_file,BLE_INIT_PACKET))){
				strcpy(fw_download_file,BLE_FW_IMAGE);
					
				clear_state(GET_REQUESTED);
				clear_state(DOWNLOADING);
				if(fw_download_url != NULL)
					http_client_send_request(&http_client_module_inst, fw_download_url, HTTP_METHOD_GET, NULL, NULL);
			} else {
				add_state(COMPLETED);
			}
			return;
		}
	}
}

/**
 * \brief Callback of the HTTP client.
 *
 * \param[in]  module_inst     Module instance of HTTP client module.
 * \param[in]  type            Type of event.
 * \param[in]  data            Data structure of the event. \refer http_client_data
 */
static void http_client_callback(struct http_client_module *module_inst, int type, union http_client_data *data)
{
	switch (type) {
	case HTTP_CLIENT_CALLBACK_SOCK_CONNECTED:
		printf("Http client socket connected\r\n");
		break;

	case HTTP_CLIENT_CALLBACK_REQUESTED:
		printf("Request completed\r\n");
		add_state(GET_REQUESTED);
		break;

	case HTTP_CLIENT_CALLBACK_RECV_RESPONSE:
		printf("Received response %u data size %u\r\n",
				(unsigned int)data->recv_response.response_code,
				(unsigned int)data->recv_response.content_length);
		if ((unsigned int)data->recv_response.response_code == 200) {
			http_file_size = data->recv_response.content_length;
			received_file_size = 0;

			if((data->recv_response.content != NULL) && (data->recv_response.content_length > 0))
				store_file_packet(data->recv_response.content, data->recv_response.content_length);
		} else {
			add_state(CANCELED);
			return;
		}

		break;

	case HTTP_CLIENT_CALLBACK_RECV_CHUNKED_DATA:

		store_file_packet(data->recv_chunked_data.data, data->recv_chunked_data.length);

		if (data->recv_chunked_data.is_complete) {
			//add_state(COMPLETED);
		}

		break;

	case HTTP_CLIENT_CALLBACK_DISCONNECTED:
		printf("Disconnected reason:%d\r\n", data->disconnected.reason);

		/* If disconnect reason is equals to -ECONNRESET(-104),
		 * It means Server was disconnected your connection by the keep alive timeout.
		 * This is normal operation.
		 */
		if (data->disconnected.reason == -EAGAIN) {
			/* Server has not responded. retry it immediately. */
			if (is_state_set(DOWNLOADING)) {
				f_close(&file_object);
				clear_state(DOWNLOADING);
			}

			if (is_state_set(GET_REQUESTED)) {
				clear_state(GET_REQUESTED);
			}

			start_download();
		}

		break;
	}
}

/**
 * \brief Initialize SD/MMC storage.
 */
#ifdef SD_MMC_ENABLE
static void init_storage_sd_mmc(void)
{
	FRESULT res;
	Ctrl_status status;

	/* Initialize SD/MMC stack. */
	sd_mmc_init();
	while (true) {
		printf("init_storage: Please plug an SD/MMC card in slot.\r\n");

		/* Wait card present and ready. */
		do {
			status = sd_mmc_test_unit_ready(0);
			if (CTRL_FAIL == status) {
				printf("init_storage: SD Card install Failed.\r\n");
				printf("init_storage: Please unplug and re-plug the card.\r\n");
				while (CTRL_NO_PRESENT != sd_mmc_check(0)) {
				}
			}
		} while (CTRL_GOOD != status);

		printf("init_storage: Mount SD card...\r\n");
		memset(&fatfs, 0, sizeof(FATFS));
		res = f_mount(LUN_ID_SD_MMC_0_MEM, &fatfs);
		if (FR_INVALID_DRIVE == res) {
			printf("init_storage: SD card Mount failed. res %d\r\n", res);
			return;
		}

		printf("init_storage: SD card Mount OK.\r\n");
		add_state(STORAGE_READY);
		return;
	}
}

#endif

#ifdef SST25VF_ENABLE
static void init_storage_data_flash(void){
	struct sst25vf_chip_config sst25vf_chip_config;
	struct spi_config sst25vf_spi_config;
	FRESULT res;
	Ctrl_status status;
	
	sst25vf_spi_get_config_defaults(&sst25vf_spi_config);
	sst25vf_spi_config.mode_specific.master.baudrate = SST25VF_CLOCK_SPEED;
	sst25vf_spi_config.mux_setting = SST25VF_SPI_PINMUX_SETTING;
	sst25vf_spi_config.pinmux_pad0 = SST25VF_SPI_PINMUX_PAD0;
	sst25vf_spi_config.pinmux_pad1 = SST25VF_SPI_PINMUX_PAD1;
	sst25vf_spi_config.pinmux_pad2 = SST25VF_SPI_PINMUX_PAD2;
	sst25vf_spi_config.pinmux_pad3 = SST25VF_SPI_PINMUX_PAD3;
	
	spi_init(&sst25vf_spi, SST25VF_SPI, &sst25vf_spi_config);
	spi_enable(&sst25vf_spi);
	
	sst25vf_chip_config.type = SST25VF_MEM_TYPE;
	sst25vf_chip_config.cs_pin = SST25VF_CS;
	
	sst25vf_chip_init(&sst25vf_chip, &sst25vf_spi, &sst25vf_chip_config);
	
	//sst25vf_chip_set_global_block_protect(&sst25vf_chip,false);
	//sst25vf_chip_erase(&sst25vf_chip);
	//sst25vf_chip_set_global_block_protect(&sst25vf_chip,true);
	
	printf("init_storage: Mount data Flash...\r\n");
	memset(&fatfs, 0, sizeof(FATFS));
	res = f_mount(LUN_ID_SST25VF_MEM, &fatfs);
	if (FR_INVALID_DRIVE == res) {
		printf("init_storage: data Flash Mount failed. res %d\r\n", res);
		return;
	}
	add_state(STORAGE_READY);
	return;
}
#endif


/**
 * \brief Configure Timer module.
 */
static void configure_timer(void)
{
	struct sw_timer_config swt_conf;
	sw_timer_get_config_defaults(&swt_conf);

	sw_timer_init(&swt_module_inst, &swt_conf);
	sw_timer_enable(&swt_module_inst);
}

/**
 * \brief Configure HTTP client module.
 */
static void configure_http_client(void)
{
	struct http_client_config httpc_conf;
	int ret;

	http_client_get_config_defaults(&httpc_conf);

	httpc_conf.recv_buffer_size = MAIN_BUFFER_MAX_SIZE;
	httpc_conf.timer_inst = &swt_module_inst;

	ret = http_client_init(&http_client_module_inst, &httpc_conf);
	if (ret < 0) {
		printf("HTTP client initialization has failed(%s)\r\n", strerror(ret));
		while (1) {
		} /* Loop forever. */
	}

	http_client_register_callback(&http_client_module_inst, http_client_callback);
}

static int wifi_event_handle(void){
	if(STA_CONNECTED != winc_get_state()){
		clear_state(WIFI_CONNECTED);
		if (is_state_set(DOWNLOADING)) {
			f_close(&file_object);
			clear_state(DOWNLOADING);
		}

		if (is_state_set(GET_REQUESTED)) {
			clear_state(GET_REQUESTED);
		}
	}
	else { // STA_CONNECTED
		if(!is_state_set(WIFI_CONNECTED)){
			add_state(WIFI_CONNECTED);
			start_download();
		}
	}
	return 0;
}

int socket_event_handle(void){
	WincMsg Msg;
	
	if(pdTRUE == xQueuePeek(socket_msg, &Msg, 0)){
		SOCKET sock = *(SOCKET*)Msg.msg;
				
		xQueueReset(socket_msg);
		//clear recv flag first 
		//then handle the message
		if(Msg.type == SOCKET_MSG_RECV){  
			tstrSocketRecvMsg msg_data;
			SockSndRecv* SndRecvMsg = (SockSndRecv*)Msg.msg;
	
			msg_data.s16BufferSize = SndRecvMsg->Bytes;
			msg_data.u16RemainingSize = SndRecvMsg->BytesRemaining;
			sock_pending_recv &= ~(1 << sock);
			xSemaphoreGive(sock_recv_event);
			http_client_socket_event_handler(sock,Msg.type,&msg_data);
		}
		else if(Msg.type == SOCKET_MSG_CONNECT){
			http_client_socket_event_handler(sock,Msg.type,(tstrSocketConnectMsg*)Msg.msg);
		}
		else if(Msg.type == SOCKET_MSG_SEND){
			SockSndRecv* SndRecvMsg = (SockSndRecv*)Msg.msg;
					
			http_client_socket_event_handler(sock,Msg.type,&(SndRecvMsg->Bytes));
		}
	}
	return 0;
}

static int resolve_event_handle(void){
	WincMsg Msg;
	
	if(pdTRUE == xQueuePeek(resolve_msg, &Msg, 0 / portTICK_PERIOD_MS)){
		uint32_t server_ip;
			
		memcpy((uint8_t*)&server_ip,Msg.msg,4);
		xQueueReset(resolve_msg);
		http_client_socket_resolve_handler(dns_host,server_ip);
	}
	return 0;
}

int update_ota_record(bool res){
	int i = 0;
	fw_ota_record_t FwOtaRecord;
	uint16_t offset = OTA_RECORD_OFFSET;
	uint16_t size = sizeof(FwOtaRecord) - sizeof(FwOtaRecord.url);
		
	//write ota record
	while(offset + size < ERASE_BLOCK_OPERATION_OFFSET){
		read_from_control_block(offset,(uint8_t*)&FwOtaRecord,size);

		for(;FwOtaTarget[i] != NULL;i++){
			if(!strcmp(FwOtaRecord.target,FwOtaTarget[i])){
				break;
			}
		}
		//if not match any valid target, can write to this part memory
		if((!strcmp(FwOtaRecord.target,fw_ota.target)) || (FwOtaTarget[i] == NULL)){
			strcpy(FwOtaRecord.target,fw_ota.target);
			FwOtaRecord.report = true;
			FwOtaRecord.result = res;
			FwOtaRecord.version = fw_ota.version;
			write_to_control_block(offset,(uint8_t*)&FwOtaRecord,size);
			break;
		}

		offset += size;
	}
}
int do_ota(void){
	
	init_state();
	/* Initialize the Timer. */
	configure_timer();
	/* Initialize the HTTP client service. */
	configure_http_client();
	/* Initialize storage. */
#ifdef SD_MMC_ENABLE
	init_storage_sd_mmc();
#endif
#ifdef SST25VF_ENABLE
	init_storage_data_flash();
#endif

	fw_download_file = fw_ota.url + strlen(fw_ota.url);
	if(!strcmp(fw_ota.target,"Main Unit")){
		
		strcpy(fw_download_file,MAIN_UNIT_FW_NAME);
		
		//If the firmware file already exists, rename previously downloaded
		//as roll-back firmware and delete previously roll-back firmware
		if(is_exist_file(&file_object,(char*)MAIN_UNIT_FW_NAME_RB)){
			f_unlink((char*)MAIN_UNIT_FW_NAME_RB);
			printf("delete previous Main Unit roll-back firmware\r\n");
		}
		if(is_exist_file(&file_object,(char*)MAIN_UNIT_FW_NAME)){
			f_rename((char*)MAIN_UNIT_FW_NAME,(char*)MAIN_UNIT_FW_NAME_RB);
			printf("rename previous downloaded Main Unit firmware as roll-back\r\n");
			//f_unlink((char*)"main-unit.bin");
		}
		
	}
	else if((!strcmp(fw_ota.target,"BLE Central")) || (!strcmp(fw_ota.target,"Door Sensor"))){
		
		strcpy(fw_download_file,BLE_INIT_PACKET);
		
		if(is_exist_file(&file_object,(char*)BLE_INIT_PACKET)){
			f_unlink((char*)BLE_INIT_PACKET);
			printf("delete previous %s init packet\r\n",fw_ota.target);
		}
		
		if(is_exist_file(&file_object,(char*)BLE_FW_IMAGE)){
			f_unlink((char*)BLE_FW_IMAGE);
			printf("delete previous %s firmware image\r\n",fw_ota.target);
		}
	}
	else{
		add_state(CANCELED);
	}

	
	fw_download_url = fw_ota.url;

	while (!(is_state_set(COMPLETED) || is_state_set(CANCELED))) {
		//check wifi connection
		wifi_event_handle();
		//handle socket event
		socket_event_handle();
		//handle DNS resolve event
		resolve_event_handle();
		/* Checks the timer timeout. */
		sw_timer_task(&swt_module_inst);
	}
	
    /* release HTTP download resource*/
	http_client_close(&http_client_module_inst);
	http_client_unregister_callback(&http_client_module_inst);
	http_client_deinit(&http_client_module_inst);
	
	if(is_state_set(COMPLETED)){
		
		if(!strcmp(fw_ota.target,"Main Unit")){
			uint8_t flag = BOOTLOADER_OTA;
			//write ota flag
			write_to_control_block(BOOTLOADER_OFFSET,&flag,1);
			printf("system reset to upgrade Main Unit FW to %d.%d\r\n",
				fw_ota.version/10,fw_ota.version%10);
			
			/* updata OTA record to nv memory*/
			update_ota_record(false);
			system_reset();
			
		} else if((!strcmp(fw_ota.target,"BLE Central")) || (!strcmp(fw_ota.target,"Door Sensor"))){
			int file_state = -1;
			uint32_t cnt = 0;
			UINT put_ptr;
		#define TEMP_SIZE    1024
			uint8_t* temp_buff = NULL;
		    char* p = NULL;
			enum dfu_object obj;
			char* file_name[2] = {BLE_INIT_PACKET,BLE_FW_IMAGE};
			bool dfu_res = true;

			temp_buff = malloc(TEMP_SIZE * sizeof(uint8_t));
			if(temp_buff == NULL)
				return -1;

			for(int i = 0;i < 2;i++){
				file_state = f_open(&file_object,(char*)file_name[i], (BYTE)FA_READ);
				if(FR_OK == file_state){
					p = strrchr(file_name[i], '.');
					p++;
					if(p != NULL)
						if(*p == 'b')  // ".bin"
							obj = data_obj;
						if(*p == 'd')  // ".dat"
							obj = cmd_obj;
					
					cnt = 0;
					while(cnt < file_object.fsize){
						f_read(&file_object,temp_buff,TEMP_SIZE, &put_ptr);
						cnt += put_ptr;

						if(0 != ble_dfu(obj,file_object.fsize,temp_buff,put_ptr)){
							printf("BLE object[%d] DFU fail\r\n",obj);
							dfu_res = false;
							break;
						}
					}
					
					f_close(&file_object);
				}
			}	
			free(temp_buff);
			printf("upgrade %s FW to version %d.%d %s\r\n",(char*)&fw_ota.target[0],
				    fw_ota.version/10,fw_ota.version%10,dfu_res ? "true" : "false");
			update_ota_record(dfu_res);
			
		} 
	}
	return 0;
}

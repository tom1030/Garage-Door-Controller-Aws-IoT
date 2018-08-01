/*
 * ota.h
 *
 * Created: 21/8/2017 下午 2:52:45
 *  Author: NSC
 */ 


#ifndef DOWNLOAD_FW_H_
#define DOWNLOAD_FW_H_


#ifdef __cplusplus
extern "C" {
#endif

#define BOOTLOADER_OTA			0x45
#define BOOTLOADER_ROLLBACK		0x46

/** Maximum size for packet buffer. */
#define MAIN_BUFFER_MAX_SIZE                 (1446)
/** Maximum file name length. */
#define MAIN_MAX_FILE_NAME_LENGTH            (64)
/** Maximum file extension length. */
#define MAIN_MAX_FILE_EXT_LENGTH             (8)
/** Output format with '0'. */
#define MAIN_ZERO_FMT(SZ)                    (SZ == 4) ? "%04d" : (SZ == 3) ? "%03d" : (SZ == 2) ? "%02d" : "%d"
/** Maximum url length. */
#define MAIN_MAX_URL_LENGTH            (128)

typedef enum {
	NOT_READY = 0, /*!< Not ready. */
	STORAGE_READY = 0x01, /*!< Storage is ready. */
	WIFI_CONNECTED = 0x02, /*!< Wi-Fi is connected. */
	GET_REQUESTED = 0x04, /*!< GET request is sent. */
	DOWNLOADING = 0x08, /*!< Running to download. */
	COMPLETED = 0x10, /*!< Download completed. */
	CANCELED = 0x20 /*!< Download canceled. */
} download_state;

typedef struct{
	char target[20];
	uint16_t version;
	bool result;
	bool report;
	char url[MAIN_MAX_URL_LENGTH];  //URL NOT record
}fw_ota_record_t;


extern fw_ota_record_t fw_ota;
	
int do_ota(void);
int socket_event_handle(void);
int update_ota_record(bool res);


#ifdef __cplusplus
}
#endif

#endif /* DOWNLOAD_FW_H_ */

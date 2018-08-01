 /*
 * shadow.c
 *
 * Created: 8/7/2017 上午 11:27:40
 *  Author: NSC
 */ 
	 
#include <inttypes.h>

#include "app.h" 
#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"
#include "aws_iot_shadow_json.h"

#include "winc.h"
#include "btn.h"
#include "wifi_config.h"
#include "download_fw.h"
#include "nv_eeprom.h"

#include "periphs.h"

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 250
#define MAX_LENGTH_OF_OTA_JSON_BUFFER 150
#define MAX_LENGTH_OF_DOOROPEN_JSON_BUFFER 100

//global data for DooeOepn
static char DoorOpen[MAX_LENGTH_OF_DOOROPEN_JSON_BUFFER] = "";

//global data for FW OTA
static char FwOta[MAX_LENGTH_OF_OTA_JSON_BUFFER] = "";

//notice target name not longer than 20B
const char* FwOtaTarget[] = {
    "Main Unit",  
	"WINC",       
	"BLE Central", 
	"Door Sensor",
	NULL		
};

static const char* FwOtaKey[] = {
	"fw version",
	"load url",
	NULL
};

static JsonPrimitiveType FwOtaType[] = {
	SHADOW_JSON_UINT16,   // --> "fw version" ,example 12 --> 1.2
	SHADOW_JSON_STRING,   // --> "load url"
};


static uint16_t fw_version[] = {0,0,0,0,};

static const char* DoorOpenTarget[] = {
    "DoorOpen_A",  
	"DoorOpen_B",       
	"DoorOpen_C", 
	"DoorOpen_D",
	NULL		
};

static uint16_t DoorOpenCH[] = {
	CH_NUMBER_A_ADDR,
	CH_NUMBER_B_ADDR,
	CH_NUMBER_C_ADDR,
	CH_NUMBER_D_ADDR,
};


//bit flag for shadow update
#define DoorOpenReport (1 << 0)
#define DoorOpenDesire (1 << 1) 
#define OtaReport      (1 << 2)
#define OtaReportResult      (1 << 3)

//default report the status of DoorOpen when power-on
static uint8_t report_bit = 0x00;

static bool ota_after_report = false;
static bool ota_now = false;

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
								const char *pReceivedJsonDocument, void *pContextData) {
	uint8_t report_flag;

	IOT_UNUSED(pThingName);
	IOT_UNUSED(action);
	//IOT_UNUSED(pReceivedJsonDocument);
	IOT_UNUSED(pContextData);

	if(SHADOW_ACK_TIMEOUT == status) {
		IOT_INFO("Update Timeout--");
	} else if(SHADOW_ACK_REJECTED == status) {
		IOT_INFO("Update Rejected !!");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		IOT_INFO("Update Accepted !!");
	}
	
	report_flag = *(uint8_t*)pContextData;
	if((report_flag & OtaReport)&&ota_after_report){
		ota_after_report = false;
		ota_now = true;
	}
}

void ShadowGetStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
								const char *pReceivedJsonDocument, void *pContextData) {
	IOT_UNUSED(pThingName);
	IOT_UNUSED(action);
	//IOT_UNUSED(pReceivedJsonDocument);
	IOT_UNUSED(pContextData);

	if(SHADOW_ACK_TIMEOUT == status) {
		IOT_INFO("Get Timeout--");
	} else if(SHADOW_ACK_REJECTED == status) {
		IOT_INFO("Get Rejected !!");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		IOT_INFO("Get Accepted !!");
	}
}

int report_ota_result(char* pTarget,bool result){
	size_t remSizeOfJsonBuffer = 0;
	int32_t snPrintfReturn = 0;
	IoT_Error_t ret_val = SUCCESS;
	uint16_t value = 10;
	int i;

	for(i = 0;FwOtaTarget[i] != NULL;i++){
		if(!(strcmp(pTarget,FwOtaTarget[i])))
			break;
	}
	
	if(FwOtaTarget[i] == NULL)
		return -1;
	
	//report result
	remSizeOfJsonBuffer = MAX_LENGTH_OF_OTA_JSON_BUFFER;
	snPrintfReturn = snprintf(FwOta, remSizeOfJsonBuffer, "{\"%s\":{\"%s\":",
			pTarget,"result");
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer)) {
		return -2;
	}

	//"true"
	remSizeOfJsonBuffer -= snPrintfReturn;
	snPrintfReturn = snprintf(FwOta + strlen(FwOta),remSizeOfJsonBuffer, "%s}}", result ? "true" : "false");
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer))
		return -3;
	report_bit |= OtaReportResult;
	return 0;
}


bool extractTokenData(const char * pJsonDocument,jsonStruct_t * pStruct,int32_t* pDataPosition,
	uint32_t* pDataLength){
	
	int32_t tokenCount;
	void *pJsonHandler = NULL;
	
	if(!isJsonValidAndParse(pJsonDocument, pJsonHandler, &tokenCount)) {
		IOT_WARN("Received JSON is not valid");
		return false;
	}

	if(isJsonKeyMatchingAndUpdateValue(pJsonDocument, pJsonHandler, tokenCount,
		pStruct, pDataLength, pDataPosition)) {
		
		if((pStruct->type == SHADOW_JSON_STRING) || (pStruct->type == SHADOW_JSON_OBJECT)){
			char* ptr = (char*)pStruct->pData;
			memcpy(ptr,pJsonDocument + *pDataPosition,*pDataLength);
			ptr[*pDataLength] = '\0';
		}
		return true;
			
	}
	return false;
}
void doorActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	jsonStruct_t pStruct;
	uint16_t Operate;
	
	int32_t DataPosition;
	uint32_t DataLength;

	if(pContext != NULL) {
		if(JsonStringDataLen > MAX_LENGTH_OF_DOOROPEN_JSON_BUFFER)
			return;
		
		memcpy(DoorOpen,pJsonString,JsonStringDataLen);
		DoorOpen[JsonStringDataLen] = '\0';
		for(int i = 0;DoorOpenTarget[i] != NULL;i++){
			pStruct.cb = NULL;
			pStruct.pKey = DoorOpenTarget[i];
			pStruct.type = SHADOW_JSON_UINT16;
			pStruct.pData = &Operate;

			if(!extractTokenData(DoorOpen,&pStruct,&DataPosition,&DataLength))
				continue;
			IOT_INFO("Delta - [%s] state changed to %d", DoorOpenTarget[i],Operate);
			if((Operate == 50) || (Operate == 100)){
				send_ts_cmd(DoorOpenCH[i],TS_CMD_ON_IMME_DI);
				send_ts_cmd(DoorOpenCH[i],TS_CMD_POLL_STATE);
				
			} else if(Operate == 0){
				send_ts_cmd(DoorOpenCH[i],TS_CMD_OFF_IMME);
				send_ts_cmd(DoorOpenCH[i],TS_CMD_POLL_STATE);
			} else
				return;
				
			if(!light_task[LAMP].onoff){

				light_task[LAMP].onoff = true;
				light_task[LAMP].case_bit = 0x33;// 00110011
				light_task[LAMP].case_going = 0;
				light_task[LAMP].duration_ms = 5000;
				init_timer(&light_task[LAMP].flash_tmr);
				countdown_ms(&light_task[LAMP].flash_tmr,250);
			}
			if(!buzz_task.onoff){
				buzz_task.onoff = true;
				buzz_task.duration_ms = 5000;
				init_timer(&buzz_task.snd_tmr);
				countdown_ms(&buzz_task.snd_tmr,5000);
				buzz_onoff(true);
			}
			report_bit |= DoorOpenReport;
		}
		
	}
}

void otaActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	jsonStruct_t pStruct;
	
	char pStrTarget[20 + MAIN_MAX_URL_LENGTH] = "";
	char pStr[MAIN_MAX_URL_LENGTH] = "";
	
	uint16_t version = 0;
	int32_t DataPosition;
	uint32_t DataLength;
	
	if(pContext != NULL) {
		if(JsonStringDataLen > MAX_LENGTH_OF_OTA_JSON_BUFFER)
			return;
		memcpy(FwOta,pJsonString,JsonStringDataLen);
		FwOta[JsonStringDataLen] = '\0';
		
		for(int i = 0;FwOtaTarget[i] != NULL;i++){
			pStruct.cb = NULL;
			pStruct.pKey = FwOtaTarget[i];
			pStruct.type = SHADOW_JSON_OBJECT;
			pStruct.pData = pStrTarget;

			if(!extractTokenData(FwOta,&pStruct,&DataPosition,&DataLength))
				continue;
			
			IOT_INFO("OTA Target : %s",FwOtaTarget[i]);
			
			//"load url" "fw version"
			for(int j = 0;FwOtaKey[j] != NULL;j++){
				pStruct.cb = NULL;
				pStruct.pKey = FwOtaKey[j];
				pStruct.type = FwOtaType[j];
				
				if(pStruct.pKey == "fw version")
					pStruct.pData = &version;
				else if(pStruct.pKey == "load url")
					pStruct.pData = pStr;
				
				if(!extractTokenData(pStrTarget,&pStruct,&DataPosition,&DataLength))
					continue;
				
				if(pStruct.pKey == "load url"){
					IOT_INFO("%s : %s",FwOtaKey[j],pStr);
					strcpy(fw_ota.url,pStr);
				}
				else if(pStruct.pKey == "fw version"){
					IOT_INFO("%s : %d.%d",FwOtaKey[j], version/10, version%10);
					fw_ota.version = version;
				}
				
			}
			
			//"Target"
			if(sizeof(FwOtaTarget[i]) <= sizeof(fw_ota.target))
				strcpy(fw_ota.target,FwOtaTarget[i]);

			//only do ota when increased fw version and valid url
			if((strlen(fw_ota.url) > 0)&&(fw_ota.version > fw_version[i])){
				ota_after_report = true;
			}

			break;
		}
		report_bit |= OtaReport;
	}
	
}

void iot_delete_action_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	IOT_INFO("delete action");
	IOT_INFO("%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, params->payload);
}

static void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} else {
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

int update_fw_version(void){
	fw_ota_record_t FwOtaRecord;
	uint16_t offset = OTA_RECORD_OFFSET;
	uint16_t size = sizeof(FwOtaRecord) - sizeof(FwOtaRecord.url);

	while(offset + size < ERASE_BLOCK_OPERATION_OFFSET){
		int i = 0;
		read_from_control_block(offset,(uint8_t*)&FwOtaRecord,size);
		
		for(;FwOtaTarget[i] != NULL;i++){
			if(!strcmp(FwOtaRecord.target,FwOtaTarget[i])){
				fw_version[i] = FwOtaRecord.version;
				IOT_INFO("%s FW version %d.%d",FwOtaRecord.target,fw_version[i]/10,fw_version[i]%10);
				break;
			}
		}
		
		//if not match any valid target need to clear this part memory
		if(FwOtaTarget[i] == NULL){
			memset((uint8_t*)&FwOtaRecord,0,size);
			write_to_control_block(offset,(uint8_t*)&FwOtaRecord,size);
		}
		if(FwOtaRecord.report == true){
			int ret = -1;
			bool res = true;
			
			if(!strcmp(FwOtaRecord.target,"Main Unit")){
				if(fw_version[i] != FW_VERSION){
					fw_version[i] = FW_VERSION;
					FwOtaRecord.version = FW_VERSION;
					IOT_INFO("%s FW ota fail, running FW version %d.%d",FwOtaRecord.target,fw_version[i]/10,fw_version[i]%10);
					res = false;
				}
			} else {
				res = FwOtaRecord.result;
			}
			FwOtaRecord.report = false;
			write_to_control_block(offset,(uint8_t*)&FwOtaRecord,size);
			
			ret = report_ota_result((char*)FwOtaRecord.target,res);
			if(ret == 0)
				IOT_INFO("Report %s FW ota result",FwOtaRecord.target);
				
			break;
		}
		
		offset += size;
	}
	return 0;
}

int update_dooropen(void){
	size_t remSizeOfJsonBuffer = 0;
	int32_t snPrintfReturn = 0;
	
	remSizeOfJsonBuffer = MAX_LENGTH_OF_DOOROPEN_JSON_BUFFER;
	
	snPrintfReturn = snprintf(DoorOpen,remSizeOfJsonBuffer, "{");
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer))
		return -2;
	
	for(int i = 0;DoorOpenTarget[i] != NULL;i++){
		snPrintfReturn = snprintf(DoorOpen + strlen(DoorOpen), remSizeOfJsonBuffer, "\"%s\":%d,",
			DoorOpenTarget[i],0);
		if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer)) {
			return -1;
		}
		remSizeOfJsonBuffer -= snPrintfReturn;
	}
	snPrintfReturn = snprintf(DoorOpen + strlen(DoorOpen) - 1,remSizeOfJsonBuffer, "}");
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer))
		return -2;
	report_bit |= DoorOpenReport;
	return 0;
}

int dooropen_desire(const char* door,const uint8_t percent){
	size_t remSizeOfJsonBuffer = 0;
	int32_t snPrintfReturn = 0;
	bool valid_door = false;

	for(int i = 0;DoorOpenTarget[i] != NULL;i++){
		if(!strncmp(door,DoorOpenTarget[i],strlen(DoorOpenTarget[i]))){
			valid_door = true;
			break;
		}
	}

	if((!valid_door) && ((percent != 0) || (percent != 50) || (percent != 100)))
		return -1;
	
	remSizeOfJsonBuffer = MAX_LENGTH_OF_DOOROPEN_JSON_BUFFER;
	
	snPrintfReturn = snprintf(DoorOpen,remSizeOfJsonBuffer, "{");
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer))
		return -2;

	snPrintfReturn = snprintf(DoorOpen + strlen(DoorOpen), remSizeOfJsonBuffer, "\"%s\":%d,",
		door,percent);
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer)) {
		return -2;
	}
	remSizeOfJsonBuffer -= snPrintfReturn;

	snPrintfReturn = snprintf(DoorOpen + strlen(DoorOpen) - 1,remSizeOfJsonBuffer, "}");
	if((snPrintfReturn < 0) ||((size_t) snPrintfReturn >= remSizeOfJsonBuffer))
		return -2;
	report_bit |= DoorOpenDesire;
	return 0;
}

int shadow_init(void) {
	IoT_Error_t rc = FAILURE;
	// initialize the mqtt client
	AWS_IoT_Client mqttClient;
	char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
	size_t sizeOfJsonDocumentBuffer = MAX_LENGTH_OF_UPDATE_JSON_BUFFER * sizeof(char);

	TickType_t tick;
	uint8_t winc_mac[12 + 1];
	uint8_t* ptr = NULL;
	uint8_t ClientId[10];  //client id format like "GDC_FFFFFF"

	uint8_t updata_callback_context;
	
	ShadowInitParameters_t sp = ShadowInitParametersDefault;
	sp.pHost = AWS_IOT_MQTT_HOST;
	sp.port = AWS_IOT_MQTT_PORT;
	sp.pClientCRT = "";//"clientCRT";
	sp.pClientKey = "";//"clientKey";
	sp.pRootCA = "";//"rootCA";
	sp.enableAutoReconnect = false;
	sp.disconnectHandler = disconnectCallbackHandler;

	
	rc = aws_iot_shadow_init(&mqttClient, &sp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Connection Error %d",rc);
		return rc;
	}

	winc_mac_address(winc_mac);
	winc_mac[12] = '\0';
	//use winc nm_common.c
	m2m_memcpy(ClientId,AWS_IOT_MQTT_CLIENT_ID,strlen(AWS_IOT_MQTT_CLIENT_ID));
	ptr = m2m_strstr(ClientId,"FFFFFF");
	m2m_memcpy(ptr,winc_mac+6,7);  //last 6 byte and '\0'

	ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
	scp.pMyThingName = AWS_IOT_GDC_THING_NAME;
	scp.pMqttClientId = ClientId;//AWS_IOT_MQTT_CLIENT_ID;
	scp.mqttClientIdLen = (uint16_t) strlen(ClientId);//(uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	//scp.deleteActionHandler = iot_delete_action_handler;
	
	rc = aws_iot_shadow_connect(&mqttClient, &scp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Connection Error %d",rc);
		return rc;
	}
	IOT_INFO("AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);
	/*
	 *  Enable Auto Reconnect functionality. 
	 *  Minimum and Maximum time of Exponential backoff are 
	 *  set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_shadow_set_autoreconnect_status(&mqttClient, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}
	
	jsonStruct_t DoorOperator;
	DoorOperator.cb = doorActuate_Callback;
	DoorOperator.pData = DoorOpen;
	DoorOperator.pKey = "DoorOpen";
	DoorOperator.type = SHADOW_JSON_OBJECT;

	jsonStruct_t OtaOperator;
	OtaOperator.cb = otaActuate_Callback;
	OtaOperator.pData = FwOta;
	OtaOperator.pKey = "FirmwareOTA";
	OtaOperator.type = SHADOW_JSON_OBJECT;

	update_fw_version();
	update_dooropen();
	
	rc = aws_iot_shadow_register_delta(&mqttClient, &DoorOperator);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Register Delta Error %d",rc);
	}

	rc = aws_iot_shadow_register_delta(&mqttClient, &OtaOperator);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Register Delta Error %d",rc);
	}
	
	// loop and publish a change
	while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
		rc = aws_iot_shadow_yield(&mqttClient, 200);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			//wait for 1000ms and switch task context
			tick = xTaskGetTickCount();
			vTaskDelayUntil(&tick,1000 / portTICK_PERIOD_MS);
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}
		
		if(report_bit){
			rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
			if(SUCCESS == rc) {
				updata_callback_context = report_bit;
				if(report_bit & DoorOpenDesire){
					report_bit &= ~DoorOpenDesire;
					rc = aws_iot_shadow_add_desired(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 1, &DoorOperator);
				}
				else if(report_bit & DoorOpenReport){
					report_bit &= ~DoorOpenReport;
					rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 1, &DoorOperator);
				}
				else if((report_bit & OtaReport) || (report_bit & OtaReportResult)){
					report_bit &= ~OtaReport;
					report_bit &= ~OtaReportResult;
					rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 1, &OtaOperator);
				}
				if(SUCCESS == rc) {
					rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
					if(SUCCESS == rc) {
						rc = aws_iot_shadow_update(&mqttClient, AWS_IOT_GDC_THING_NAME, JsonDocumentBuffer,
											   ShadowUpdateStatusCallback, (void*)&updata_callback_context, 4, true);
					}
				}
			}
		}
		
		if(ota_now)
			break;
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop %d", rc);
	}

	IOT_INFO("Disconnecting");
	rc = aws_iot_shadow_disconnect(&mqttClient);
	if(SUCCESS != rc) {
		IOT_ERROR("Disconnect error %d", rc);
	}
	
	return rc;
}

int ota_check(void){
	if(ota_now){
		ota_now = false;
		
		if(!strcmp(fw_ota.target,"WINC")){
			winc_ota(fw_ota.url);
		} else {
			do_ota();
		}
	}
	return 0;
}

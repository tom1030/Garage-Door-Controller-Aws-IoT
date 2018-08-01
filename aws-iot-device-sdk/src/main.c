/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \main page User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * This is a bare minimum user application template.
 *
 * For documentation of the board, go \ref group_common_boards "here" for a link
 * to the board-specific documentation.
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */
#include <asf.h>
#include <stdio.h>

#include "winc.h"
#include "timer_interface.h"
#include "app.h"
#include "btn.h"
#include "wifi_config.h"
#include "nv_eeprom.h"
#include "ble_serial_cmd.h"
#include "dfu_serial.h"
#include "periphs.h"

#include "cjson.h"
#include "download_fw.h"


#define STRING_EOL    "\r\n"
#define STRING_HEADER STRING_EOL"-- Garage Door Controller --"STRING_EOL \
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Firmware version "FW_VERSION_STRING" --"STRING_EOL \
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

/** UART module for debug. */
static struct usart_module cdc_uart_module;

TaskHandle_t xAwsTaskHandle = NULL;

/**
 *  Configure UART console.
 */
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
#if 1
	usart_conf.mux_setting = EXT1_UART_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EXT1_UART_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EXT1_UART_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EXT1_UART_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EXT1_UART_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;
	
	stdio_serial_init(&cdc_uart_module, EXT1_UART_MODULE, &usart_conf);
	while (usart_init(&cdc_uart_module,
				EXT1_UART_MODULE, &usart_conf) != STATUS_OK) {
	}
	usart_enable(&cdc_uart_module);
#else
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;
	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);

	while (usart_init(&ble_uart_module,
				EDBG_CDC_MODULE, &usart_conf) != STATUS_OK) {
	}
	usart_enable(&ble_uart_module);
#endif

}

static void configure_wdt(void)
{
	/* Create a new configuration structure for the Watchdog settings and fill
	 * with the default module settings. */
	struct wdt_conf config_wdt;

	wdt_get_config_defaults(&config_wdt);

	config_wdt.always_on      = false;
#if !((SAML21) || (SAMC21) || (SAML22) || (SAMR30))
	config_wdt.clock_source   = GCLK_GENERATOR_4;
#endif
	config_wdt.timeout_period = WDT_PERIOD_16384CLK;
	config_wdt.early_warning_period = WDT_PERIOD_8192CLK;


	/* Initialize and enable the Watchdog with the user settings */
	wdt_set_config(&config_wdt);
}

void _watchdog_early_warning_callback(void){
	/* Feed watch dog */
	wdt_reset_count();
}

static void configure_wdt_callbacks(void){
	wdt_register_callback(_watchdog_early_warning_callback,
		WDT_CALLBACK_EARLY_WARNING);
	wdt_enable_callback(WDT_CALLBACK_EARLY_WARNING);
}

#ifdef _ATE_

static const char* ate_cmd[] = {
	"led",
	"lamp",
	"buzz",
	"ble",
	"wifi",
	"aws",
	"firmware",
	"reset",
	NULL,
};

static void _Ping_Cb(uint32 u32IPAddr, uint32 u32RTT, uint8 u8ErrorCode){
	printf(">Reply from %d.%d.%d.%d: TTL = %d,ErrorCode = %d\r\n",u32IPAddr & 0xFF,
		(u32IPAddr & 0xFF00) >> 8,(u32IPAddr & 0xFF0000) >> 16,
		(u32IPAddr & 0xFF000000) >> 24,u32RTT,u8ErrorCode);
	//PING_ERR_SUCCESS
	//PING_ERR_DEST_UNREACH
	//PING_ERR_TIMEOUT
}

static void _ate_cmd_prase(char const *cmd){
	char *ptr;
	int i;
	
	if((cmd == NULL) || (strlen(cmd) <= 0))
		return;
	ptr = cmd;
	
	if((strncmp(ptr, "ate ", strlen("ate ")) != 0) &&
		(strncmp(ptr, "ATE ", strlen("ATE ")) != 0)){
		printf(">Command MUST start with \"ate \" or \"ATE \": %s",cmd);
		printf(">Command format: ate [cmd] [param] [...]\r\n");
		printf(">Command support: ");
		for(i = 0; ate_cmd[i] != NULL;i++)
			printf("%s ",ate_cmd[i]);
		printf("\r\n");
		return;
	}
	ptr += strlen("ate") + 1; //+ \r
	
	for(i = 0;ate_cmd[i] != NULL;i++){
		if(!strncmp(ptr, ate_cmd[i], strlen(ate_cmd[i]))){
			ptr += strlen(ate_cmd[i]) + 1; // + \r
			if(!strncmp(ate_cmd[i],"led",strlen("led"))){
				if(!strncmp(ptr, "R_ON",strlen("R_ON"))){
					LED_On(LED_R_PIN);
				}
				else if(!strncmp(ptr, "R_OFF", strlen("R_OFF"))){
					LED_Off(LED_R_PIN);
				}
				else if(!strncmp(ptr, "G_ON", strlen("G_ON"))){
					LED_On(LED_G_PIN);
				}
				else if(!strncmp(ptr, "G_OFF" ,strlen("G_OFF"))){
					LED_Off(LED_G_PIN);
				}
				else
					printf(">Invalid Params for led, support \"R_ON\" \"R_OFF\" \"G_ON\" \"G_OFF\"\r\n");
				break;
			}
			if(!strncmp(ate_cmd[i],"lamp",strlen("lamp"))){
				if(!strncmp(ptr, "ON",strlen("ON"))){
					lamp_onoff(true);
				}
				else if(!strncmp(ptr, "OFF", strlen("OFF"))){
					lamp_onoff(false);
				}
				else
					printf(">Invalid Params for lamp, support \"ON\" \"OFF\"\r\n");
				
				break;
			}
			if(!strncmp(ate_cmd[i],"buzz",strlen("buzz"))){
				if(!strncmp(ptr, "ON",strlen("ON"))){
					buzz_onoff(true);
				}
				else if(!strncmp(ptr, "OFF", strlen("OFF"))){
					buzz_onoff(false);
				}
				else
					printf(">Invalid Params for buzz, support \"ON\" \"OFF\"\r\n");
				break;
			}
			if(!strncmp(ate_cmd[i],"wifi",strlen("wifi"))){
				if(!strncmp(ptr, "connect",strlen("connect"))){
					char *ssid,*key,*ptr_end;
					uint32_t timeoutMs;
					ptr += strlen("connect") + 1; // + \r
					ptr += 1;  // skip '['

					ptr_end = strstr(ptr, "]");
					if(ptr_end == NULL) {
						printf(">wifi connect [ssid] [password] [timeout],need [...]\r\n");
						return;
					}
					*ptr_end = '\0';
					ssid = ptr;

					ptr = ptr_end + 3; // skip "] ["
					ptr_end = strstr(ptr, "]");
					if(ptr_end == NULL) {
						printf(">wifi connect [ssid] [password] [timeout],need [...]\r\n");
						return;
					}
					*ptr_end = '\0';
					key = ptr;

					ptr = ptr_end + 3; // skip "] ["
					ptr_end = strstr(ptr, "]");
					if(ptr_end == NULL) {
						printf(">wifi connect [ssid] [password] [timeout],need [...]\r\n");
						return;
					}
					*ptr_end = '\0';
					timeoutMs = atoi(ptr);

					if(STA_CONNECTED == winc_get_state()){
						winc_disconnect(10000); // 10s timeout
					}
					winc_connect(ssid,M2M_WIFI_SEC_WPA_PSK,key,timeoutMs);
				}
				else if(!strncmp(ptr, "ping", strlen("ping"))){
					char *url,*ptr_end;

					if(STA_CONNECTED != winc_get_state()){
						printf(">Not connect to any AP\r\n");
						return;
					}
					ptr += strlen("ping") + 1;
					ptr_end = strstr(ptr, "\r\n");
					if(ptr_end == NULL) return;
					*ptr_end = '\0';
					url = ptr;
					
					winc_ping(url,_Ping_Cb);
				}
				else if(!strncmp(ptr, "RSSI", strlen("RSSI"))){
					if(STA_CONNECTED != winc_get_state()){
						printf(">Not connect to any AP\r\n");
						return;
					}
					winc_rssi();
				}
				else if(!strncmp(ptr, "down", strlen("down"))){
					if(STA_CONNECTED != winc_get_state()){
						printf(">Not connect to any AP\r\n");
						return;
					}
					winc_disconnect(1000);
				}
				else if(!strncmp(ptr, "TX_PWR", strlen("TX_PWR"))){
					
				}
				else if(!strncmp(ptr, "ip", strlen("ip"))){
					uint32_t ip;
					if(STA_CONNECTED != winc_get_state()){
						printf(">Not connect to any AP\r\n");
						return;
					}
					ip = winc_get_ip();
					printf(">%u.%u.%u.%u\r\n",(ip&0xFF000000)>>24,
						(ip&0xFF0000)>>16,(ip&0xFF00)>>8,
						(ip&0xFF));
				}
				else if(!strncmp(ptr, "ap", strlen("ap"))){
					ConnInfo _conn_info;
					
					if(STA_CONNECTED != winc_get_state()){
						printf(">Not connect to any AP\r\n");
						return;
					}
					m2m_memset((uint8*)&_conn_info,0,sizeof(_conn_info));
					read_from_control_block(WIFI_CONFIG_OFFSET,(uint8_t*)&_conn_info,sizeof(_conn_info));
	
					printf(">%s\r\n",_conn_info.ssid);
				}
				else if(!strncmp(ptr, "mac", strlen("mac"))){
					uint8_t winc_mac[12 + 1];
					printf(">");
					winc_mac_address(winc_mac);
				}
				else
					printf(">Invalid Params for wifi, support \"connnect\" \"ping\" \"RSSI\" \"down\" \"TX_PWR\" \"ip\" \"ap\" \"mac\"\r\n");
				break;
			}

			if(!strncmp(ate_cmd[i],"ble",strlen("ble"))){
				cJSON *root = NULL;	
				char* out = NULL;	
				char *ptr_end;
				uint8_t sen;
					
				root = cJSON_CreateObject();
				if(!strncmp(ptr, "SEN_ON",strlen("SEN_ON"))){

					ptr += strlen("SEN_ON") + 1; // + \r
					
					ptr_end = strstr(ptr, "\r\n");
					if(ptr_end == NULL) return;
					*ptr_end = '\0';
					sen = atoi(ptr);

					if(sen == 1){
						cJSON_AddStringToObject(root, "GD first", "ON");	
					} else if(sen == 2){
						cJSON_AddStringToObject(root, "GD second", "ON");
					}
				}
				else if(!strncmp(ptr, "SEN_OFF",strlen("SEN_OF"))){
					ptr += strlen("SEN_OFF") + 1; // + \r

					ptr_end = strstr(ptr, "\r\n");
					if(ptr_end == NULL) return;
					*ptr_end = '\0';
					sen = atoi(ptr);

					if(sen == 1){
						cJSON_AddStringToObject(root, "GD first", "OFF");	
					} else if(sen == 2){
						cJSON_AddStringToObject(root, "GD second", "OFF");
					}
				}
				else {
					printf(">Invalid Params for ble, support \"SEN_ON\" \"SEN_OFF\"\r\n");
				}

				out = cJSON_Print(root);
				if(out != NULL){	
					printf("*%s#",out); //* + json cmd + #	
					printf("\r\n");
					free(out);
				}
				cJSON_Delete(root);
				break;
			}

			if(!strncmp(ate_cmd[i],"aws",strlen("aws"))){
				if(!strncmp(ptr, "shadow",strlen("shadow"))){
				#include "aws_iot_config.h"
					printf(">%s\r\n",AWS_IOT_MQTT_HOST);
				}
				else if(!strncmp(ptr, "DoorOpen",strlen("DoorOpen"))){
					char *door,*ptr_end;
					uint8_t per;
					char DoorOpen[11];
					
					ptr += strlen("DoorOpen") + 1; // + \r

					ptr_end = strstr(ptr, " ");
					if(ptr_end == NULL) {
						printf(">aws DoorOpen door percent\r\n");
						return;
					}
					*ptr_end = '\0';
					door = ptr;
					
					ptr = ptr_end + 1;
					ptr_end = strstr(ptr, "\r\n");
					if(ptr_end == NULL) {
						printf(">aws DoorOpen door percent\r\n");
						return;
					}
					*ptr_end = '\0';
					per = atoi(ptr);

					memcpy(DoorOpen,"DoorOpen_X\0",sizeof("DoorOpen_X\0"));
					ptr = strstr(DoorOpen,"X");
					*ptr = *door;
					if(dooropen_desire(DoorOpen,per) < 0){
						printf(">Invalid Params for gdc DoorOpen, support \"A\" \"B\" \"C\" \"D\" and open percent 0,50,100\r\n");
					}
				}
				else
					printf(">Invalid Params for aws, support \"shadow\" \"DoorOpen\"\r\n");
				break;
			}
			
			if(!strncmp(ate_cmd[i], "firmware",strlen("firmware"))){
				uint8_t rev[3];
				printf(">Main Unit firmware version: %s\r\n",FW_VERSION_STRING);
				winc_fw_version(rev);
				printf(">Wi-Fi firmware version: %d.%d.%d\r\n",rev[0],rev[1],rev[2]);
				printf(">BLE Central firmware version: %d.%d\r\n",0,0);
				printf(">Door Sensor firmware version: %d.%d\r\n",0,0);
				break;
			}

			if(!strncmp(ate_cmd[i], "reset",strlen("reset"))){
				system_reset();
				//never get here
			}
		}
	}

	if(ate_cmd[i] == NULL){
		printf(">Invalid cmd, only support: ");
		for(i = 0; ate_cmd[i] != NULL;i++)
			printf("%s ",ate_cmd[i]);
		printf("\r\n");
	}
}

#endif
static int _ble_cmd_prase(char const *cmd){
	cJSON *json = NULL;

	if(cmd == NULL)
		return -1;
	
	json = cJSON_Parse(cmd);
	if (!json){
		printf("JSON prase error before: [%s]\n", cJSON_GetErrorPtr());
		return 0;
	} else {
		cJSON *pr_out = NULL;
		cJSON *sr_out = NULL;
	
		pr_out = cJSON_GetObjectItem(json,"Position Report");
		if(pr_out != NULL){
			cJSON *opened = NULL;
			cJSON *pos = NULL;
	
			opened = cJSON_GetObjectItem(pr_out,"opened");
			if(opened != NULL){
			if(opened->type == cJSON_True)
				printf("Door Sensor Report : Door Fully Opened\r\n");
			}
	
			pos = cJSON_GetObjectItem(pr_out,"position");
			if(pos != NULL){
				if(pos->type == cJSON_Number)
					printf("Door Sensor Report : Door Position %d\r\n",pos->valueint);
				}
			}
	
			sr_out = cJSON_GetObjectItem(json,"Sensor Report");
			if(sr_out != NULL){
				cJSON *batt = NULL;
				cJSON *temp = NULL;
	
				batt = cJSON_GetObjectItem(sr_out,"battery level");
				if(batt != NULL){
					if(batt->type == cJSON_Number)
						printf("Door Sensor Report : battery level %d\r\n",batt->valueint);
				}
				temp = cJSON_GetObjectItem(sr_out,"temperature");
				if(temp != NULL){
					if(temp->type == cJSON_Number)
						printf("Door Sensor Report : temperature %d\r\n",temp->valueint);
				}
			}
				
			cJSON_Delete(json);
			json = NULL;
	}
	return 1;
	
	/*
	cJSON *root = NULL; 
	char* out = NULL;	
	cJSON *pr = NULL;	
	cJSON *sr = NULL;	
	root = cJSON_CreateObject();	
	cJSON_AddItemToObject(root, "Position Report", pr = cJSON_CreateObject());	
	//cJSON_AddFalseToObject(pr, "opened");  
	//fully open -> true, not fully open -> false	
	//cJSON_AddFalseToObject(pr, "closed");  
	//fully close -> true, not fully close -> false 
	cJSON_AddTrueToObject(pr, "in motion"); //moving -> true, not move -> false 
	cJSON_AddTrueToObject(pr, "motion going"); //up -> true, down -> false	
	cJSON_AddNumberToObject(pr, "position", 20);  //open percent off the ground 
	//cJSON_AddItemToObject(root, "Sensor Report", sr = cJSON_CreateObject());	
	//cJSON_AddNumberToObject(sr, "battery level", 100);	
	//cJSON_AddNumberToObject(sr, "temperature", 25);	
	out = cJSON_Print(root);	
	cJSON_Delete(root);
	printf("%s",out); //* + json cmd + #	
	free(out);	
	out = NULL;
	*/
}

/* Idle task will handle below tasks:
   1. Ble command polling;
   2. LED and alarm lamp continuously flashing;
   3. Alarm buzzer continuously sound;
   4. Feed watch dog
*/


struct light_flash light_task[light_id_max];
struct buzz_snd buzz_task;

static void misc_task(void *pvParameters){
	/* declare a few. */
#define CMD_BUFF_SIZE 128
	uint8_t cmd[CMD_BUFF_SIZE];
	uint16_t cnt = 0;
	uint16_t temp;
	BtnState state_btn;
	
	while(1){
		state_btn = btn_new_state();
		if(state_btn == BTN_SHORT_PRESS){  
			
		}
		else if(state_btn == BTN_LONG_PRESS){ 
			vTaskSuspend(xAwsTaskHandle);
			//long press to do wifi config
			do_wifi_config();
			vTaskResume(xAwsTaskHandle);
		}
		/* Ble command polling */
		if (usart_read_wait(&ble_uart_module, &temp) == STATUS_OK) {
			//usart_write_wait(&cdc_uart_module, temp);
			if(temp == '*'){
				cnt = 0;
			}
			else if(temp == '#'){
				//printf("%s",cmd);
				_ble_cmd_prase(cmd);
				cnt = 0;
				memset(cmd,0,CMD_BUFF_SIZE);
			}
			else{
				cmd[cnt] = (uint8_t)temp;
				cnt++;
				if(cnt >= CMD_BUFF_SIZE){
					cnt = 0;
					memset(cmd,0,CMD_BUFF_SIZE);
				}
			}
		}
	#ifdef _ATE_
		//ATE command 
		if (usart_read_wait(&cdc_uart_module, &temp) == STATUS_OK) {
			usart_write_wait(&cdc_uart_module, temp);
			vTaskSuspend(xAwsTaskHandle);
			if(temp == '\n'){
				cmd[cnt] = (uint8_t)temp;
				_ate_cmd_prase(cmd);
				cnt = 0;
				memset(cmd,0,CMD_BUFF_SIZE);
				usart_write_wait(&cdc_uart_module, '>');
			} else if(temp == 0x08){ //backspace
				if(cnt > 0){
					cnt -= 1;
					usart_write_wait(&cdc_uart_module, 0x20); //space
					usart_write_wait(&cdc_uart_module, 0x08);
				}
			} else{
				cmd[cnt] = (uint8_t)temp;
				//if((cmd[0] == 0x1B)&&(!strncmp(&cmd[1], "[A",strlen("[A")))){
				//	cnt = 0;
				//	memset(cmd,0,CMD_BUFF_SIZE);
				//	continue;
				//}
				cnt++;
				if(cnt >= CMD_BUFF_SIZE){
					printf("\r\n>Only support ate command less than 128 Bytes\r\n");
					cnt = 0;
					memset(cmd,0,CMD_BUFF_SIZE);
				    usart_write_wait(&cdc_uart_module, '>');
				}
			} 
			vTaskResume(xAwsTaskHandle);
		}
	#endif
		/* LED and alarm lamp continuously flashing */
		for(int i = 0;i < light_id_max;i++){
			if((light_task[i].onoff)&&(light_task[i].duration_ms > 0)){
				if(has_timer_expired(&light_task[i].flash_tmr)){
					if(light_task[i].case_bit & (1 << light_task[i].case_going)){
						if(i == LED_R)
							LED_On(LED_R_PIN);
						if(i == LED_G)
							LED_On(LED_G_PIN);
						if(i == LAMP)
							lamp_onoff(true);
					} else {
						if(i == LED_R)
							LED_Off(LED_R_PIN);
						if(i == LED_G)
							LED_Off(LED_G_PIN);
						if(i == LAMP)
							lamp_onoff(false);
					}
					
					light_task[i].case_going += 1;
					if(light_task[i].case_going > 7)
						light_task[i].case_going = 0;
					
					light_task[i].duration_ms -= 250;
					if(light_task[i].duration_ms > 0){
						init_timer(&light_task[i].flash_tmr);
						countdown_ms(&light_task[i].flash_tmr,250);
					} else {
						//complete the task
						light_task[i].onoff = false;
					}
					
				}
			}
		}
		
		/* Alarm buzzer continuously sound */
		if(buzz_task.onoff){
			if(has_timer_expired(&buzz_task.snd_tmr)){
				buzz_task.onoff = false;
				buzz_onoff(false);
			}
		}
	}
}

static void aws_task(void *pvParameters){
	int ret = -1;
	BtnState state_btn;
	ConnInfo _conn_info;
	
	m2m_memset((uint8*)&_conn_info,0,sizeof(_conn_info));
	read_from_control_block(WIFI_CONFIG_OFFSET,(uint8_t*)&_conn_info,sizeof(_conn_info));
	ret = winc_connect(_conn_info.ssid,M2M_WIFI_SEC_WPA_PSK,_conn_info.password,60000);
	
	while(1){
		if(STA_CONNECTED == winc_get_state()){
			if(0 == shadow_init()){ // return 'SUCCESS' means OTA normally break
				ota_check();
			}
			
		}
		else{
			//connect to AP fail when startup, delay for switch task
			//and re-check the winc state
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
	}
}

static void m2m_wifi_task(void *pvParameters)
{
	while(1){	
		xSemaphoreTake(sem_m2m_event, portMAX_DELAY);
		m2m_wifi_handle_events(pvParameters);
	}
}

int main (void){
	struct eeprom_emulator_parameters parameters;
	enum system_reset_cause reset_cause;

	//d21 platform init
	system_init();
	delay_init();

	/* Insert application code here, after the board has been initialized. */
	/* Initialize the UART console. */
	configure_console();
	printf(STRING_HEADER);
	
	configure_wdt();
	configure_wdt_callbacks();
	reset_cause = system_get_reset_cause();
	if (reset_cause == SYSTEM_RESET_CAUSE_WDT) {
		printf("\r\nSystem reset caused by watch-dog!!!!!\r\n\r\n");
	}
	//nv memory init
	init_control_block(&parameters);
	ble_config_uart();
	winc_init();
	
	solid_relay_init();
	lamp_init();
	buzz_init();

	xTaskCreate(misc_task, "misc", configMINIMAL_STACK_SIZE * 5, NULL, tskIDLE_PRIORITY + 1, NULL);
	xTaskCreate(aws_task, "aws", configMINIMAL_STACK_SIZE * 10, NULL, tskIDLE_PRIORITY + 2, &xAwsTaskHandle);	
	xTaskCreate(m2m_wifi_task, "m2m_wifi", configMINIMAL_STACK_SIZE * 5, NULL, tskIDLE_PRIORITY + 3, NULL);
	vTaskStartScheduler();
	while (1) {
	}
}


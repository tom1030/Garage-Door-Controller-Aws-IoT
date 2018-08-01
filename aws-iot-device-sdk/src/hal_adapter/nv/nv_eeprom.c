/*
* nv_eeprom.c
*
* Created: 21/8/2017 上午 11:37:57
*  Author: NSC
*/ 

#include <asf.h>
#include "nv_eeprom.h"


void init_control_block(struct eeprom_emulator_parameters* parameters){
	/* Setup EEPROM emulator service */
	enum status_code error_code = eeprom_emulator_init();
	if (error_code == STATUS_ERR_NO_MEMORY) {
		while (true) {
			/* No EEPROM section has been set in the device's fuses */
		}
	}
	else if (error_code != STATUS_OK) {
		/* Erase the emulated EEPROM memory (assume it is unformatted or
		* irrecoverably corrupt) */
		eeprom_emulator_erase_memory();
		eeprom_emulator_init();
	}
	eeprom_emulator_get_parameters(parameters);
}

int write_to_control_block(const uint16_t offset,const uint8_t *const data, const uint16_t length){
	enum status_code error_code;

	error_code = eeprom_emulator_write_buffer(offset,data,length);
	if(error_code != STATUS_OK)
		return -1;
	error_code = eeprom_emulator_commit_page_buffer();
	if(error_code != STATUS_OK)
		return -2;
	return 0;
}

int read_from_control_block(const uint16_t offset,uint8_t * data, const uint16_t length){
	enum status_code error_code = eeprom_emulator_read_buffer(offset,data,length);
	if(error_code != STATUS_OK)
		return -1;
	return 0;
}

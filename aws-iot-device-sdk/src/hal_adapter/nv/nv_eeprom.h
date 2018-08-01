/*
 * nv_eeprom.h
 *
 * Created: 21/8/2017 上午 11:38:25
 *  Author: NSC
 */ 


#ifndef NV_EEPROM_H_
#define NV_EEPROM_H_

#ifdef __cplusplus
extern "C" {
#endif


/*
**   eeprom usage map
*/

#define BOOTLOADER_OFFSET 0      //bootloader using 0
#define OTA_RECORD_OFFSET 1   // 1 ~ 100
#define ERASE_BLOCK_OPERATION_OFFSET 100   // 100 ~ 140 
#define WIFI_CONFIG_OFFSET 140     // wifi config data using 140 ~ EEPROM size

void init_control_block(struct eeprom_emulator_parameters* parameters);

int write_to_control_block(const uint16_t offset,const uint8_t *const data, const uint16_t length);

int read_from_control_block(const uint16_t offset,uint8_t * data, const uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* NV_EEPROM_H_ */

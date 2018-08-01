/*
 * sst25vf_mem.c
 *
 * Created: 15/8/2017 下午 5:54:56
 *  Author: NSC
 */ 

 //_____  I N C L U D E S ___________________________________________________

#include "conf_access.h"


#if SST25VF_MEM == ENABLE

#include "conf_sst25vf.h"
#include "sst25vf.h"
#include "sst25vf_mem.h"

#include "nv_eeprom.h"

extern struct sst25vf_chip_module sst25vf_chip;

typedef struct{
	uint32_t SectorMap[8];
	uint8_t UseBit; // 1 used, 0 not use
}erase_block_operation_t;

static erase_block_operation_t e_block;


//_____ D E F I N I T I O N S ______________________________________________

/*! \name Control Interface
 */
//! @{

static bool b_sst25vf_unloaded = false;

Ctrl_status sst25vf_test_unit_ready(void)
{
	Ctrl_status ret;

	if (b_sst25vf_unloaded) {
		return CTRL_NO_PRESENT;
	}

	ret = (sst25vf_chip_check_presence(&sst25vf_chip) == STATUS_OK) ? CTRL_GOOD : CTRL_NO_PRESENT;
	return ret;
}


Ctrl_status sst25vf_read_capacity(U32 *u32_nb_sector)
{
	/* FATFS sector size 512 Bytes. */	
	/* reserve 8 sector or 8 * 512 = 4096 bytes for write cache*/
	*u32_nb_sector = (SST25VF_MEM_CNT << (SST25VF_MEM_SIZE - 9)) - 1 - 8;
	return CTRL_GOOD;
}


bool sst25vf_wr_protect(void)
{
	return false;
}


bool sst25vf_removal(void)
{
	return false;
}


//! @}


#if ACCESS_MEM_TO_RAM == true

/*! \name MEM <-> RAM Interface
 */
//! @{


#define FATFS_SECTOR_SIZE 512
#define ERASE_BLOCK_SIZE 4096

Ctrl_status sst25vf_df_2_ram(U32 addr, void *ram)
{
	uint32_t sector_addr;
	uint32_t block_address_e;
	
	/* FATFS sector size 512 Bytes. */
	if (addr + 1 + 8 > SST25VF_MEM_CNT << (SST25VF_MEM_SIZE - 9)){
		return CTRL_FAIL;
	}

	read_from_control_block(ERASE_BLOCK_OPERATION_OFFSET,(uint8_t*)&e_block,sizeof(e_block));
	
	//printf("read sector %d\r\n",addr);
	//check the erase-block first
	for(int i = 0;i < ERASE_BLOCK_SIZE / FATFS_SECTOR_SIZE;i++){
		if((e_block.UseBit & (1 << i)) && (e_block.SectorMap[i] == addr)){
			block_address_e = ERASE_BLOCK_SIZE * (1 << (SST25VF_MEM_SIZE - 12) - 1);
			sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, false);
			if(STATUS_OK != sst25vf_chip_read_buffer(&sst25vf_chip, block_address_e + i * FATFS_SECTOR_SIZE, (uint32_t*)ram, FATFS_SECTOR_SIZE)){
				return CTRL_FAIL;
			}
			sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, true);
			//printf("catch in erase-block, read %d\r\n",i);
			return CTRL_GOOD;
		}
	}
	
	sector_addr = addr * FATFS_SECTOR_SIZE;
	sst25vf_chip_set_sector_protect(&sst25vf_chip, sector_addr, false);
	//read 1 sector(512 Bytes) from df to ram
	if(STATUS_OK != sst25vf_chip_read_buffer(&sst25vf_chip, sector_addr, (uint32_t*)ram, FATFS_SECTOR_SIZE)){
		return CTRL_FAIL;
	}
	sst25vf_chip_set_sector_protect(&sst25vf_chip, sector_addr, true);

	return CTRL_GOOD;
}

Ctrl_status sst25vf_ram_2_df(U32 addr, const void *ram)
{
	uint8_t sectors_each_block = ERASE_BLOCK_SIZE / FATFS_SECTOR_SIZE;  //8
	uint32_t block_address_e,block_address_w;
	uint8_t Cache[FATFS_SECTOR_SIZE];
	bool erase_write_back = false;
	
	/* FATFS sector size 512 Byte. */
	if (addr + 1 + 8 > SST25VF_MEM_CNT << (SST25VF_MEM_SIZE - 9)) {
		return CTRL_FAIL;
	}

	read_from_control_block(ERASE_BLOCK_OPERATION_OFFSET,(uint8_t*)&e_block,sizeof(e_block));
	
	//printf("\r\n+++++++++write to sector %d\r\n",addr);
	//printf("e_block use bit: 0x%x\r\n",e_block.UseBit);
	//printf("e_block sector map: %d %d %d %d %d %d %d %d\r\n",e_block.SectorMap[0],
	//	e_block.SectorMap[1],e_block.SectorMap[2],e_block.SectorMap[3],
	//	e_block.SectorMap[4],e_block.SectorMap[5],e_block.SectorMap[6],
	//	e_block.SectorMap[7]);
	
	block_address_e = ERASE_BLOCK_SIZE * (1 << (SST25VF_MEM_SIZE - 12) - 1);
	sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, false);

	if(e_block.UseBit == 0)
		sst25vf_chip_erase_block(&sst25vf_chip, block_address_e, SST25VF_BLOCK_SIZE_4KB);

	for(int i = 0;i < sectors_each_block;i++){
		if(!(e_block.UseBit & (1 << i))){ //not used and the sector shoud just be erased and can be wrote
			if(i == 0){
				e_block.SectorMap[i] = addr;
				e_block.UseBit |= 1 << i;
				sst25vf_chip_write_buffer(&sst25vf_chip,block_address_e + i * FATFS_SECTOR_SIZE,ram, FATFS_SECTOR_SIZE);
				//printf("store to erase block sector [%d]\r\n",i);
			
				break;
			}
			else{ //i > 0
				if(e_block.SectorMap[i-1] / sectors_each_block == addr / sectors_each_block){  //same 4k erase block
					e_block.SectorMap[i] = addr;
					e_block.UseBit |= 1 << i;
					sst25vf_chip_write_buffer(&sst25vf_chip,block_address_e + i * FATFS_SECTOR_SIZE,ram, FATFS_SECTOR_SIZE);
					//printf("store to erase block sector [%d]\r\n",i);
			
					break;
				}
				else{  
					//not in same 4k erase block, need to write back to write-block
					//and write ram to erase-block
					erase_write_back = true;
					//printf("break, need to write back and erase 1\r\n\r\n");
					break;
				}
			}
		}
		else{  //used
			if((e_block.UseBit == 0xFF) || (e_block.SectorMap[i] == addr)){
				erase_write_back = true;
				//printf("break, need to write back and erase 2\r\n\r\n");
				break;
			}
			continue;
			
		}
	}

	if(erase_write_back){
		uint8_t UseBit_w = 0;
		uint8_t UseBit = 0;

		block_address_w = ERASE_BLOCK_SIZE * (e_block.SectorMap[0] / sectors_each_block);
		
		sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_w, false);

		for(int i = 0;i < sectors_each_block;i++){
			if(e_block.UseBit & (1 << i)){
				UseBit_w |= (1 << e_block.SectorMap[i] % sectors_each_block);
				UseBit += 1;
			}
		}
		//printf("UseBit_w 0x%x\r\n",UseBit_w);
		for(int i = 0;i < sectors_each_block;i++){
			if(!(UseBit_w & (1 << i))){
				sst25vf_chip_read_buffer(&sst25vf_chip, block_address_w + i * FATFS_SECTOR_SIZE,Cache, FATFS_SECTOR_SIZE);
				
				sst25vf_chip_write_buffer(&sst25vf_chip,block_address_e + UseBit * FATFS_SECTOR_SIZE,Cache, FATFS_SECTOR_SIZE);		

				e_block.SectorMap[UseBit] = i + sectors_each_block * (e_block.SectorMap[0] / sectors_each_block);

				UseBit += 1;
				
			}
		}
		e_block.UseBit = 0xFF;
		
		sst25vf_chip_erase_block(&sst25vf_chip, block_address_w, SST25VF_BLOCK_SIZE_4KB);

		for(int i = 0; (e_block.UseBit & (1 << i))&&(i < sectors_each_block);i++){

			sst25vf_chip_read_buffer(&sst25vf_chip, block_address_e + i * FATFS_SECTOR_SIZE,Cache, FATFS_SECTOR_SIZE);
			
			if(e_block.SectorMap[i] == addr){
				sst25vf_chip_write_buffer(&sst25vf_chip,block_address_w + 
					FATFS_SECTOR_SIZE * (e_block.SectorMap[i] % sectors_each_block),
					ram, FATFS_SECTOR_SIZE);
				erase_write_back = false;
			}
			else{
				sst25vf_chip_write_buffer(&sst25vf_chip,block_address_w + 
					FATFS_SECTOR_SIZE * (e_block.SectorMap[i] % sectors_each_block),
					Cache, FATFS_SECTOR_SIZE);
			}
			//printf("----------write back to sector [%d]\r\n",e_block.SectorMap[i]);
		}

		sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_w, true);

		sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, false);
		//Erase cache block 
		sst25vf_chip_erase_block(&sst25vf_chip, block_address_e, SST25VF_BLOCK_SIZE_4KB);	
		sst25vf_chip_read_buffer(&sst25vf_chip, block_address_e,Cache, FATFS_SECTOR_SIZE);
		if(erase_write_back){ 
			e_block.SectorMap[0] = addr;
			e_block.UseBit = 1 << 0;
			//write ram to erase-block
			sst25vf_chip_write_buffer(&sst25vf_chip,block_address_e, ram, FATFS_SECTOR_SIZE);
		}
		else{
			e_block.UseBit = 0;
		}
	}
	
	sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, true);
	
	write_to_control_block(ERASE_BLOCK_OPERATION_OFFSET,(uint8_t*)&e_block,sizeof(e_block));
	
	return CTRL_GOOD;
}


//! @}

#endif  // ACCESS_MEM_TO_RAM == true

#endif  // AT45DBX_MEM == ENABLE

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

extern struct sst25vf_chip_module sst25vf_chip;

typedef struct{
	
}block_operation_t;
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


Ctrl_status sst25vf_df_2_ram(U32 addr, void *ram)
{
	uint32_t sector_addr;
	
	/* FATFS sector size 512 Bytes. */
	if (addr + 1 + 8 > SST25VF_MEM_CNT << (SST25VF_MEM_SIZE - 9)){
		return CTRL_FAIL;
	}

	sector_addr = addr * 512;
	sst25vf_chip_set_sector_protect(&sst25vf_chip, sector_addr, false);

	//read 1 sector(512 Bytes) from df to ram
	if(STATUS_OK != sst25vf_chip_read_buffer(&sst25vf_chip, sector_addr, (uint32_t*)ram, 512)){
		return CTRL_FAIL;
	}
	
	sst25vf_chip_set_sector_protect(&sst25vf_chip, sector_addr, true);

	return CTRL_GOOD;
}

#define SMALL_CACHE_SIZE 512
#define ERASE_BLOCK_SIZE 4096

Ctrl_status sst25vf_ram_2_df(U32 addr, const void *ram)
{
	uint8_t sectors_each_block = ERASE_BLOCK_SIZE / 512;  //8
	uint8_t caches_each_sector = 512 / SMALL_CACHE_SIZE;  
	uint32_t block_address_e,block_address_w;
	uint8_t Cache[SMALL_CACHE_SIZE];
	
	/* FATFS sector size 512 Byte. */
	if (addr + 1 + 8 > SST25VF_MEM_CNT << (SST25VF_MEM_SIZE - 9)) {
		return CTRL_FAIL;
	}

	//Erase cache block 
	block_address_e = ERASE_BLOCK_SIZE * (1 << (SST25VF_MEM_SIZE - 12) - 1);
	sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, false);
	sst25vf_chip_erase_block(&sst25vf_chip, block_address_e, SST25VF_BLOCK_SIZE_4KB);
	
	block_address_w = ERASE_BLOCK_SIZE * (addr / sectors_each_block);
	sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_w, false);

	//remove write block data to cache block before erase write block
	for(int i = 0; i< ERASE_BLOCK_SIZE / SMALL_CACHE_SIZE;i++){
		sst25vf_chip_read_buffer(&sst25vf_chip, block_address_w + i * SMALL_CACHE_SIZE,Cache, SMALL_CACHE_SIZE);
		sst25vf_chip_write_buffer(&sst25vf_chip,block_address_e + i * SMALL_CACHE_SIZE,Cache, SMALL_CACHE_SIZE);
	}		
	sst25vf_chip_erase_block(&sst25vf_chip, block_address_w, SST25VF_BLOCK_SIZE_4KB);
	
	//write ram data and rest offset cache block data into write block
	for(int i = 0; i< ERASE_BLOCK_SIZE / SMALL_CACHE_SIZE;i++){
		if((i * SMALL_CACHE_SIZE >= 512 * (addr % sectors_each_block)) && 
			(i * SMALL_CACHE_SIZE < 512 * (addr % sectors_each_block + 1))){
			//update each 128 bytes in 1 sector(512 Bytes) to cache
			memcpy(Cache,(uint8_t*)(ram + SMALL_CACHE_SIZE * (i % caches_each_sector)),SMALL_CACHE_SIZE);
			
		}
		else{			
			sst25vf_chip_read_buffer(&sst25vf_chip, block_address_e + i * SMALL_CACHE_SIZE,Cache, SMALL_CACHE_SIZE);
		}
		
		sst25vf_chip_write_buffer(&sst25vf_chip,block_address_w + i * SMALL_CACHE_SIZE,Cache, SMALL_CACHE_SIZE);
	}
	
	sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_e, true);
	sst25vf_chip_set_sector_protect(&sst25vf_chip, block_address_w, true);
	
	return CTRL_GOOD;
}


//! @}

#endif  // ACCESS_MEM_TO_RAM == true


#endif  // AT45DBX_MEM == ENABLE

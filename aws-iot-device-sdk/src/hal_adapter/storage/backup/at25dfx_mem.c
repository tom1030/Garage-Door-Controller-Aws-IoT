/*
 * at25dfx_mem.c
 *
 * Created: 15/8/2017 下午 5:54:56
 *  Author: NSC
 */ 

 //_____  I N C L U D E S ___________________________________________________

#include "conf_access.h"


#if AT25DFX_MEM == ENABLE

#include "conf_at25dfx.h"
#include "at25dfx.h"
#include "at25dfx_mem.h"

extern struct at25dfx_chip_module at25dfx_chip;

//_____ D E F I N I T I O N S ______________________________________________

/*! \name Control Interface
 */
//! @{

static bool b_at25dfx_unloaded = false;

Ctrl_status at25dfx_test_unit_ready(void)
{
	Ctrl_status ret;

	if (b_at25dfx_unloaded) {
		return CTRL_NO_PRESENT;
	}

	at25dfx_chip_wake(&at25dfx_chip);
	ret = (at25dfx_chip_check_presence(&at25dfx_chip) == STATUS_OK) ? CTRL_GOOD : CTRL_NO_PRESENT;
	at25dfx_chip_sleep(&at25dfx_chip);
	return ret;
}


Ctrl_status at25dfx_read_capacity(U32 *u32_nb_sector)
{
	/* FATFS sector size 512 Bytes. */	
	*u32_nb_sector = (AT25DFX_MEM_CNT << (AT25DFX_MEM_SIZE - 9)) - 1;
	return CTRL_GOOD;
}


bool at25dfx_wr_protect(void)
{
	return false;
}


bool at25dfx_removal(void)
{
	return false;
}


//! @}


#if ACCESS_MEM_TO_RAM == true

/*! \name MEM <-> RAM Interface
 */
//! @{


Ctrl_status at25dfx_df_2_ram(U32 addr, void *ram)
{
	uint16_t sector_addr;
	
	/* FATFS sector size 512 Bytes. */
	if (addr + 1 > AT25DFX_MEM_CNT << (AT25DFX_MEM_SIZE - 9)){
		return CTRL_FAIL;
	}

	sector_addr = addr * 512;
	at25dfx_chip_wake(&at25dfx_chip);
	at25dfx_chip_set_sector_protect(&at25dfx_chip, sector_addr, false);

	//read 1 sector(512 Bytes) from df to ram
	if(STATUS_OK != at25dfx_chip_read_buffer(&at25dfx_chip, sector_addr, (uint32_t*)ram, 512)){
		return CTRL_FAIL;
	}
	
	at25dfx_chip_set_sector_protect(&at25dfx_chip, sector_addr, true);
	at25dfx_chip_sleep(&at25dfx_chip);

	return CTRL_GOOD;
}


Ctrl_status at25dfx_ram_2_df(U32 addr, const void *ram)
{
	uint8_t sectors_each_block = 4096 / 512;
	uint16_t block_address;

	/* FATFS sector size 512 Byte. */
	if (addr + 1 > AT25DFX_MEM_CNT << (AT25DFX_MEM_SIZE - 9)) {
		return CTRL_FAIL;
	}
	
	block_address = 4096 * (addr / sectors_each_block);
	
	at25dfx_chip_wake(&at25dfx_chip);
	at25dfx_chip_set_sector_protect(&at25dfx_chip, block_address, false);
	
	//read 4k block data to cache before erase
	at25dfx_chip_read_buffer(&at25dfx_chip, block_address,pCache, 4096);
	
	at25dfx_chip_erase_block(&at25dfx_chip, block_address, AT25DFX_BLOCK_SIZE_4KB);

	//update 1 sector(512 Bytes) to cache
	memcpy(&pCache[(addr * 512) % 4096],(uint8_t*)ram,512);

	//write update cache data to block
	if(STATUS_OK != at25dfx_chip_write_buffer(&at25dfx_chip,block_address, pCache, 4096)){
		return CTRL_FAIL;
	}
	
	at25dfx_chip_set_sector_protect(&at25dfx_chip, block_address, true);
	at25dfx_chip_sleep(&at25dfx_chip);

	return CTRL_GOOD;
}


//! @}

#endif  // ACCESS_MEM_TO_RAM == true


#endif  // AT45DBX_MEM == ENABLE
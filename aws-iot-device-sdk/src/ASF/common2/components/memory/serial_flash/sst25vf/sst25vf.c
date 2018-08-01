/**
 * \file
 *
 * \brief SST25VF SerialFlash driver implementation.
 *
 * Copyright (C) 2013-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include "sst25vf.h"

/** SerialFlash command opcodes */
enum sst25vf_command_opcode {
	SST25VF_COMMAND_READ = 0x03,
	SST25VF_COMMAND_H_SPEED_READ = 0x0B,
	SST25VF_COMMAND_4K_SECTOR_ERASE = 0x20,
	SST25VF_COMMAND_32K_BLOCK_ERASE = 0x52,
	SST25VF_COMMAND_64K_BLOCK_ERASE = 0xD8,
	SST25VF_COMMAND_CHIP_ERASE = 0x60,
	SST25VF_COMMAND_BYTE_PROGRAM = 0x02,
	SST25VF_COMMAND_AAI = 0xAD,    //Auto Address Increment Programming
	SST25VF_COMMAND_RDSR = 0x05,   //Read-Status-Register
	SST25VF_COMMAND_EWSR = 0x50,   //Enable-Write-Status-Register
	SST25VF_COMMAND_WRSR = 0x01,   //Write-Status-Register
	SST25VF_COMMAND_WREN = 0x06,   //Write-Enable
	SST25VF_COMMAND_WRDI = 0x04,   //Write-Disable 
	SST25VF_COMMAND_RDID = 0x90,   //Read-ID
	SST25VF_COMMAND_JEDEC_ID = 0x9F,   //JEDEC-ID
	SST25VF_COMMAND_EBSY = 0x70,   // Enable SO to output RY/BY#status during AAI programming
	SST25VF_COMMAND_DBSY = 0x80    //Disable SO as RY/BY#status during AAI programming
	
};

/** Maximum length of a SerialFlash command */
#define SST25VF_COMMAND_MAX_SIZE  (1 + 3 + 2)

/** SerialFlash status bits */
enum sst25vf_status_field {
	// These two are read-fields
	SST25VF_STATUS_BUSY         = (1 << 0),
	SST25VF_STATUS_WEL          = (1 << 1),
	// These are read-write-fields
	SST25VF_STATUS_BP0          = (1 << 2),
	SST25VF_STATUS_BP1          = (1 << 3),
	SST25VF_STATUS_BP2          = (1 << 4),
	SST25VF_STATUS_BP3          = (1 << 5),
	// These two are read-fields
	SST25VF_STATUS_AAI          = (1 << 6),
	// This is a read-write-field
	SST25VF_STATUS_BPL          = (1<< 7)
};

/** SerialFlash command container */
struct sst25vf_command {
	/** Opcode to send */
	enum sst25vf_command_opcode opcode;
	/** Size: opcode byte (1) [+ address bytes (3)] [+ dummy bytes (N)] */
	uint8_t command_size;
	/** SerialFlash address to operate on */
	sst25vf_address_t address;
	/** Buffer to read from/write to */
	union {
		const uint8_t *tx;
		uint8_t *rx;
	} data;
	/** Number of bytes to read/write */
	sst25vf_datalen_t length;
};

//! \name SerialFlash chip info helpers
//@{

/**
 * \brief Get the device ID of a specific SerialFlash type.
 *
 * \param[in] type Type of SerialFlash.
 *
 * \return SerialFlash device ID.
 */
static inline uint32_t _sst25vf_get_device_id(enum sst25vf_type type)
{
	switch (type){
		case SST25VF_080B:
			return 0xBF8E; //manufacrure's ID (BFH) + device ID (8EH)
		default:
			Assert(false);
			return 0;
	}
}

/**
 * \brief Get the storage size of a specific SerialFlash type.
 *
 * \param[in] type Type of SerialFlash.
 *
 * \return SerialFlash storage size.
 */
static inline uint32_t _sst25vf_get_device_size(enum sst25vf_type type)
{
	switch (type){
		case SST25VF_080B:
			return 8 * 1024 * 1024UL;
		default:
			Assert(false);
			return 0;
	}
}

//@}

//! \name Private chip helpers
//@{

/**
 * \brief Select the chip
 *
 * This function selects the specified chip by driving its CS line low.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 */
static inline void _sst25vf_chip_select(struct sst25vf_chip_module *chip)
{
	port_pin_set_output_level(chip->cs_pin, false);
}

/**
 * \brief Deselect the chip
 *
 * This function deselects the specified chip by driving its CS line high.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 */
static inline void _sst25vf_chip_deselect(struct sst25vf_chip_module *chip)
{
	port_pin_set_output_level(chip->cs_pin, true);
}

#include <sst25vf_priv_hal.h>

/**
 * \brief Issue command to enable writing
 *
 * This function issues the command that enables operations which change the
 * SerialFlash content or operation, i.e., programming, erasing and protecting
 * or unprotecting sectors.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 */
static inline void _sst25vf_chip_enable_write(struct sst25vf_chip_module *chip)
{
	struct sst25vf_command cmd;

	cmd.opcode = SST25VF_COMMAND_WREN;
	cmd.command_size = 1;
	cmd.length = 0;

	// Init to avoid warnings with -Os
	cmd.address = (sst25vf_address_t)NULL;
	cmd.data.tx = NULL;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);
}

static inline void _sst25vf_chip_disable_write(struct sst25vf_chip_module *chip)
{
	struct sst25vf_command cmd;

	cmd.opcode = SST25VF_COMMAND_WRDI;
	cmd.command_size = 1;
	cmd.length = 0;

	// Init to avoid warnings with -Os
	cmd.address = (sst25vf_address_t)NULL;
	cmd.data.tx = NULL;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);
}


//@}

/**
 * \brief Check presence of chip
 *
 * This function checks whether or not the SerialFlash device is present by
 * attempting to read out its device ID, and comparing it with the one that
 * its type should have.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 *
 * \return Status of operation.
 * \retval STATUS_OK if chip responded with ID matching its type.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 * \retval STATUS_ERR_NOT_FOUND if chip did not respond, or with wrong ID.
 */
enum status_code sst25vf_chip_check_presence(struct sst25vf_chip_module *chip)
{
	enum status_code status;
	struct sst25vf_command cmd;
	uint32_t id = 0;

	Assert(chip);

	// Reserve the SPI for us
	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}

	cmd.opcode = SST25VF_COMMAND_RDID;
	cmd.command_size = 4;
	cmd.address = 0x000001;  //DEVICE ID address
	cmd.data.rx = (uint8_t *)&id;
	cmd.length = 2; //manufacrure's + device ID

	_sst25vf_chip_issue_read_command_wait(chip, cmd);

	_sst25vf_spi_unlock(chip->spi);
	if (id == _sst25vf_get_device_id(chip->type)) {
		return STATUS_OK;
	} else {
		return STATUS_ERR_NOT_FOUND;
	}
}

/**
 * \brief Read data from chip
 *
 * This function reads data from the SerialFlash device, into a buffer.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 * \param[in] address SerialFlash internal address to start reading from.
 * \param[out] data Buffer to write data into.
 * \param[in] length Number of bytes to read.
 *
 * \return Status of operation.
 * \retval STATUS_OK if operation succeeded.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 * \retval STATUS_ERR_INVALID_ARG if address and/or length is out of bounds.
 */
enum status_code sst25vf_chip_read_buffer(struct sst25vf_chip_module *chip,
		sst25vf_address_t address, void *data, sst25vf_datalen_t length)
{
	enum status_code status;
	struct sst25vf_command cmd;

	Assert(chip);
	Assert(data);
	Assert(length);

	// Address out of range?
	if ((address + length) > _sst25vf_get_device_size(chip->type)) {
		return STATUS_ERR_INVALID_ARG;
	}

	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}

	cmd.opcode = SST25VF_COMMAND_READ;
	cmd.command_size = 4;
	cmd.address = address;
	cmd.data.rx = (uint8_t *)data;
	cmd.length = length;
	_sst25vf_chip_issue_read_command_wait(chip, cmd);

	_sst25vf_spi_unlock(chip->spi);

	return STATUS_OK;
}

/**
 * \brief Write data to chip
 *
 * This function writes data to the SerialFlash device, from a buffer.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 * \param[in] address SerialFlash internal address to start writing to.
 * \param[in] data Buffer to read data from.
 * \param[in] length Number of bytes to write.
 *
 * \return Status of operation.
 * \retval STATUS_OK if operation succeeded.
 * \retval STATUS_ERR_IO if operation failed.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 * \retval STATUS_ERR_INVALID_ARG if address and/or length is out of bounds.
 */
enum status_code sst25vf_chip_write_buffer(struct sst25vf_chip_module *chip,
		sst25vf_address_t address, const void *data, sst25vf_datalen_t datalen)
{
	enum status_code status;
	struct sst25vf_command cmd;
	sst25vf_datalen_t length;
	
	Assert(chip);
	Assert(data);
	Assert(datalen);

	if ((address + datalen) > _sst25vf_get_device_size(chip->type)) {
		return STATUS_ERR_INVALID_ARG;
	}

	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}
	length = datalen;
	cmd.length = 0;
	
	if((length == 1) || (address % 2 == 1)){
		_sst25vf_chip_enable_write(chip);
		
		cmd.opcode = SST25VF_COMMAND_BYTE_PROGRAM;
		cmd.command_size = 4;
		cmd.address = address;
		cmd.data.tx = (uint8_t *)data;
		cmd.length = 1;
		
		_sst25vf_chip_issue_write_command_wait(chip, cmd);
		status = _sst25vf_chip_get_nonbusy_status(chip);
		length -= cmd.length;
	}
	
	if (length > 1){
		_sst25vf_chip_enable_write(chip);
		
		cmd.opcode = SST25VF_COMMAND_AAI;
		cmd.command_size = 4;
		cmd.address = address + cmd.length;
		cmd.data.tx = (uint8_t *)(data + cmd.length);
		cmd.length = 2;
		
		_sst25vf_chip_issue_write_command_wait(chip, cmd);
		status = _sst25vf_chip_get_nonbusy_status(chip);
		length -= cmd.length;
	}
	
	while ((length > 1) && (status == STATUS_OK)) {
		cmd.opcode = SST25VF_COMMAND_AAI;
		cmd.command_size = 1;
		cmd.data.tx += cmd.length;
		cmd.length = 2;

		_sst25vf_chip_issue_write_command_wait(chip, cmd);
		status = _sst25vf_chip_get_nonbusy_status(chip);
		length -= cmd.length;
	}
	
	_sst25vf_chip_disable_write(chip);	//disbale AAI

	if(length > 0){
		
		_sst25vf_chip_enable_write(chip);
		
		cmd.opcode = SST25VF_COMMAND_BYTE_PROGRAM;
		cmd.command_size = 4;
		cmd.address = address + datalen - 1;
		cmd.data.tx = (uint8_t *)(data + datalen - 1);
		cmd.length = 1;
		
		_sst25vf_chip_issue_write_command_wait(chip, cmd);
		status = _sst25vf_chip_get_nonbusy_status(chip);
		length -= cmd.length;
	}
	
	_sst25vf_spi_unlock(chip->spi);

	return status;
}

/**
 * \brief Erase chip
 *
 * This function erases all content of the SerialFlash device.
 *
 * \pre All sectors must be unprotected prior to a chip erase, or it will not be
 * performed.
 *
 * \sa sst25vf_chip_set_global_sector_protect()
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 *
 * \return Status of operation.
 * \retval STATUS_OK if operation succeeded.
 * \retval STATUS_ERR_IO if operation failed.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 */
enum status_code sst25vf_chip_erase(struct sst25vf_chip_module *chip)
{
	enum status_code status;
	struct sst25vf_command cmd;

	Assert(chip);

	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}

	_sst25vf_chip_enable_write(chip);

	cmd.opcode = SST25VF_COMMAND_CHIP_ERASE;
	cmd.command_size = 1;
	cmd.length = 0;

	// Init to avoid warnings with -Os
	cmd.address = (sst25vf_address_t)NULL;
	cmd.data.tx = NULL;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);

	status = _sst25vf_chip_get_nonbusy_status(chip);
	
	_sst25vf_spi_unlock(chip->spi);

	return status;
}

/**
 * \brief Erase block
 *
 * This function erases all content within a block of the SerialFlash device.
 *
 * \pre The sector(s) which the block resides in must be unprotected prior to a
 * block erase, or it will not be performed.
 *
 * \sa sst25vf_chip_set_sector_protect()
 *
 * \note The alignment of the erase blocks is given by the erase block size. The
 * SerialFlash device will simply ignore address bits which index within the
 * block. For example, doing a 4 kB block erase with the start address set to
 * the 2 kB boundary will cause the first 4 kB to get erased, not 4 kB starting
 * at the 2 kB boundary.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 * \param[in] address Address within the block to erase.
 * \param[in] block_size Size of block to erase.
 *
 * \return Status of operation.
 * \retval STATUS_OK if operation succeeded.
 * \retval STATUS_ERR_IO if operation failed.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 * \retval STATUS_ERR_INVALID_ARG if address is out of bounds.
 */
enum status_code sst25vf_chip_erase_block(struct sst25vf_chip_module *chip,
		sst25vf_address_t address, enum sst25vf_block_size block_size)
{
	enum status_code status;
	struct sst25vf_command cmd;

	Assert(chip);

	if (address >= _sst25vf_get_device_size(chip->type)) {
		return STATUS_ERR_INVALID_ARG;
	}

	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}

	_sst25vf_chip_enable_write(chip);

	switch (block_size) {
	case SST25VF_BLOCK_SIZE_4KB:
		cmd.opcode = SST25VF_COMMAND_4K_SECTOR_ERASE;
		break;

	case SST25VF_BLOCK_SIZE_32KB:
		cmd.opcode = SST25VF_COMMAND_32K_BLOCK_ERASE;
		break;

	case SST25VF_BLOCK_SIZE_64KB:
		cmd.opcode = SST25VF_COMMAND_64K_BLOCK_ERASE;
		break;

	default:
		Assert(false);
		cmd.opcode = (enum sst25vf_command_opcode)0;
	}
	cmd.command_size = 4;
	cmd.address = address;
	cmd.length = 0;

	// Init to avoid warnings with -Os
	cmd.data.tx = NULL;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);

	status = _sst25vf_chip_get_nonbusy_status(chip);

	_sst25vf_spi_unlock(chip->spi);

	return status;
}

/**
 * \brief Set all blocks protection globally
 *
 * This function applies a protect setting to all blocks.
 *
 * \note Global setting of block protection is done by writing to the status
 * register of the device.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 * \param[in] bool Protection setting to apply.
 * \arg \c true if the blocks should be protected.
 * \arg \c false if the blocks should be unprotected.
 *
 * \return Status of operation.
 * \retval STATUS_OK if write operation succeeded.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 */
enum status_code sst25vf_chip_set_global_block_protect(
		struct sst25vf_chip_module *chip, bool protect)
{
	enum status_code status;
	struct sst25vf_command cmd;
	uint8_t temp_data;
	uint8_t status_reg = 0;

	Assert(chip);

	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}

	status = _sst25vf_chip_get_status(chip,&status_reg);
	if(status != STATUS_OK){
		return status;
	}

	_sst25vf_chip_enable_write(chip);

	temp_data = protect ? (status_reg | SST25VF_STATUS_BP2 | SST25VF_STATUS_BP1 | SST25VF_STATUS_BP0) 
		: (status_reg & ~(SST25VF_STATUS_BP2 | SST25VF_STATUS_BP1 | SST25VF_STATUS_BP0));

	cmd.opcode = SST25VF_COMMAND_EWSR;
	cmd.command_size = 1;

	// Init to avoid warnings with -Os
	cmd.address = (sst25vf_address_t)NULL;
	cmd.length = 0;
	cmd.data.tx = NULL;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);
	
	cmd.opcode = SST25VF_COMMAND_WRSR;
	cmd.command_size = 1;
	cmd.length = 1;
	cmd.data.tx = &temp_data;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);

	_sst25vf_spi_unlock(chip->spi);

	return STATUS_OK;
}

/**
 * \brief Set protection setting of a single sector
 *
 * This function applies a protect setting to a single sector.
 *
 * \note The granularity of the sectors for protection can vary between
 * SerialFlash devices and is not necessarily uniform. Please refer to the
 * datasheet for details.
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 * \param[in] address Address within sector to protect.
 * \param[in] bool Protection setting to apply.
 * \arg \c true if the sector should be protected.
 * \arg \c false if the sector should be unprotected.
 *
 * \return Status of operation.
 * \retval STATUS_OK if write operation succeeded.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 * \retval STATUS_ERR_INVALID_ARG if address is out of bounds.
 */
enum status_code sst25vf_chip_set_sector_protect(
		struct sst25vf_chip_module *chip, sst25vf_address_t address,
		bool protect)
{
	enum status_code status;
	struct sst25vf_command cmd;
	uint8_t temp_data = 0;
	uint8_t status_reg = 0;

	Assert(chip);

	if ((address) >= _sst25vf_get_device_size(chip->type)) {
		return STATUS_ERR_INVALID_ARG;
	}

	status = _sst25vf_spi_lock(chip->spi);
	if (status == STATUS_BUSY) {
		return status;
	}

	status = _sst25vf_chip_get_status(chip,&status_reg);
	if(status != STATUS_OK){
		return status;
	}

	_sst25vf_chip_enable_write(chip);

	if((address >= 0xF0000) && (address <= 0xFFFFF))
		temp_data = SST25VF_STATUS_BP0;
	else if((address >= 0xE0000) && (address <= 0xF0000))
		temp_data = SST25VF_STATUS_BP1;
	else if((address >= 0xC0000) && (address <= 0xE0000))
		temp_data = SST25VF_STATUS_BP1 | SST25VF_STATUS_BP0;
	else if((address >= 0x80000) && (address <= 0xC0000))
		temp_data = SST25VF_STATUS_BP2;
	else
		temp_data = SST25VF_STATUS_BP2 | SST25VF_STATUS_BP1 | SST25VF_STATUS_BP0;

	temp_data = protect ?
			status_reg | temp_data : status_reg & ~temp_data;
	
	cmd.opcode = SST25VF_COMMAND_WRSR;
	cmd.command_size = 1;
	cmd.length = 1;
	cmd.data.tx = &temp_data;

	// Init to avoid warnings with -Os
	cmd.address = (sst25vf_address_t)NULL;

	_sst25vf_chip_issue_write_command_wait(chip, cmd);

	_sst25vf_spi_unlock(chip->spi);

	return STATUS_OK;
}

/**
 * \brief Get protection setting of a single sector
 *
 * This function gets the protect setting of a single sector.
 *
 * \sa sst25vf_chip_set_sector_protect()
 *
 * \param[in] chip Address of SerialFlash chip instance to operate on.
 * \param[in] address Address within sector to get setting of.
 * \param[out] bool Address of variable to store the setting to.
 *
 * \return Status of operation.
 * \retval STATUS_OK if operation succeeded.
 * \retval STATUS_BUSY if SPI is busy with some other operation.
 * \retval STATUS_ERR_INVALID_ARG if address is out of bounds.
 */
enum status_code sst25vf_chip_get_sector_protect(
		struct sst25vf_chip_module *chip, sst25vf_address_t address,
		bool *protect)
{
	enum status_code status;
	struct sst25vf_command cmd;
	uint8_t status_reg = 0;
	uint8_t temp_data = 0;

	Assert(chip);

	if ((address) >= _sst25vf_get_device_size(chip->type)) {
		return STATUS_ERR_INVALID_ARG;
	}
	
	status = _sst25vf_chip_get_status(chip,&status_reg);
	if(status != STATUS_OK){
		return status;
	}

	temp_data = SST25VF_STATUS_BP2 | SST25VF_STATUS_BP1 | SST25VF_STATUS_BP0;
	
	if((address >= 0xF0000) && (address <= 0xFFFFF)){
		if(status_reg & temp_data)
			*protect = true;
		else
			*protect = false;
	}
	else if((address >= 0xE0000) && (address <= 0xF0000)){
		if((status_reg & temp_data == SST25VF_STATUS_BP0) || 
			(status_reg & temp_data == 0))
			*protect = false;
		else
			*protect = true;
	}
	else if((address >= 0xC0000) && (address <= 0xE0000)){
		if((status_reg & temp_data == SST25VF_STATUS_BP0) || 
			(status_reg & temp_data == SST25VF_STATUS_BP1) ||
			(status_reg & temp_data == 0))
			*protect = false;
		else
			*protect = true;
	}
	else {
		if(status_reg & temp_data == SST25VF_STATUS_BP2)
			*protect = true;
		else
			*protect = false;
	}

	return STATUS_OK;
}


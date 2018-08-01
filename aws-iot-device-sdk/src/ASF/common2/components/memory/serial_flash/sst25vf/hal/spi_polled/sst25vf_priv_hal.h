/**
 * \file
 *
 * \brief AT25DFx SerialFlash driver private SPI HAL interface.
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

#ifndef SST25VF_PRIV_HAL_H
#define SST25VF_PRIV_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

//! \name Private SPI helpers
//@{

/** Alias for SPI lock function */
#define _sst25vf_spi_lock    spi_lock
/** Alias for SPI unlock function */
#define _sst25vf_spi_unlock  spi_unlock

/**
 * \brief Issue a read command
 *
 * \param chip Address of SerialFlash chip instance to operate on.
 * \param cmd The command to issue.
 */
static inline void _sst25vf_chip_issue_read_command_wait(
		struct sst25vf_chip_module *chip, struct sst25vf_command cmd)
{
	enum status_code status;
	uint8_t cmd_buffer[SST25VF_COMMAND_MAX_SIZE];

	UNUSED(status);

	Assert((cmd.command_size) && (cmd.command_size <= SST25VF_COMMAND_MAX_SIZE));

	// Construct command to send
	cmd_buffer[0] = cmd.opcode;

	if (cmd.command_size > 1) {
		Assert(cmd.command_size >= 4);

		cmd_buffer[3] = cmd.address & 0xff;
		cmd_buffer[2] = (cmd.address >> 8) & 0xff;
		cmd_buffer[1] = (cmd.address >> 16) & 0xff;
	}
	// Don't bother with init of dummy bytes

	// Issue command, then start read
	_sst25vf_chip_select(chip);

	status = spi_write_buffer_wait(chip->spi, cmd_buffer, cmd.command_size);
	Assert(status == STATUS_OK);

	if (cmd.length) {
		status = spi_read_buffer_wait(chip->spi, cmd.data.rx, cmd.length, 0);
		Assert(status == STATUS_OK);
	}

	_sst25vf_chip_deselect(chip);
}

/**
 * \brief Issue a read command
 *
 * \param chip Address of SerialFlash chip instance to operate on.
 * \param cmd The command to issue.
 */
static inline void _sst25vf_chip_issue_write_command_wait(
		struct sst25vf_chip_module *chip, struct sst25vf_command cmd)
{
	enum status_code status;
	uint8_t cmd_buffer[SST25VF_COMMAND_MAX_SIZE];

	UNUSED(status);

	Assert((cmd.command_size) && (cmd.command_size <= SST25VF_COMMAND_MAX_SIZE));

	cmd_buffer[0] = cmd.opcode;

	if (cmd.command_size > 1) {
		Assert(cmd.command_size >= 4);

		cmd_buffer[3] = cmd.address & 0xff;
		cmd_buffer[2] = (cmd.address >> 8) & 0xff;
		cmd_buffer[1] = (cmd.address >> 16) & 0xff;
	}

	_sst25vf_chip_select(chip);

	status = spi_write_buffer_wait(chip->spi, cmd_buffer, cmd.command_size);
	Assert(status == STATUS_OK);

	if (cmd.length) {
		status = spi_write_buffer_wait(chip->spi, cmd.data.tx, cmd.length);
		Assert(status == STATUS_OK);
	}

	_sst25vf_chip_deselect(chip);
}

/**
 * \brief Get status after current operation completes
 *
 * This function will issue a command to read out the status, and will then read
 * the status continuously from the chip until it indicates that it is not busy.
 * The error flag of the status is then checked, before returning the result.
 *
 * \param chip Address of SerialFlash chip instance to operate on.
 *
 * \return Status of the operation.
 * \retval STATUS_OK if operation succeeded.
 * \retval STATUS_ERR_IO if an error occurred.
 */
static inline enum status_code _sst25vf_chip_get_nonbusy_status(
		struct sst25vf_chip_module *chip)
{
	enum status_code status;
	uint16_t status_reg = 0;

	UNUSED(status);

	// Issue status read command
	while (!spi_is_ready_to_write(chip->spi)) {
	}

	_sst25vf_chip_select(chip);
	status = spi_write(chip->spi, SST25VF_COMMAND_RDSR);
	Assert(status == STATUS_OK);

	while (!spi_is_ready_to_read(chip->spi)) {
	}
	status = spi_read(chip->spi, &status_reg);
	Assert(status == STATUS_OK);

	// Keep reading until busy flag clears
	// TODO: Add some timeout functionality here!
	do {
		// Do dummy writes to read out status
		while (!spi_is_ready_to_write(chip->spi)) {
		}
		status = spi_write(chip->spi, 0);
		Assert(status == STATUS_OK);

		while (!spi_is_ready_to_read(chip->spi)) {
		}
		status = spi_read(chip->spi, &status_reg);
		Assert(status == STATUS_OK);
	} while (status_reg & SST25VF_STATUS_BUSY);

	_sst25vf_chip_deselect(chip);

	return STATUS_OK;
}


static inline enum status_code _sst25vf_chip_get_status(
		struct sst25vf_chip_module *chip,uint8_t* status_reg)
{
	enum status_code status;

	UNUSED(status);

	// Issue status read command
	while (!spi_is_ready_to_write(chip->spi)) {
	}

	_sst25vf_chip_select(chip);
	status = spi_write(chip->spi, SST25VF_COMMAND_RDSR);
	Assert(status == STATUS_OK);

	while (!spi_is_ready_to_read(chip->spi)) {
	}
	status = spi_read(chip->spi, status_reg);
	Assert(status == STATUS_OK);

	_sst25vf_chip_deselect(chip);

	return STATUS_OK;
}

//@}

#ifdef __cplusplus
}
#endif

#endif // SST25VF_PRIV_HAL_H

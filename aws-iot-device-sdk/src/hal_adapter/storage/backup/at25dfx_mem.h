/*
 * at25dfx_mem.h
 *
 * Created: 15/8/2017 下午 5:55:26
 *  Author: NSC
 */ 


#ifndef AT25DFX_MEM_H_
#define AT25DFX_MEM_H_


#include "conf_access.h"

#if AT25DFX_MEM == DISABLE
  #error at25dfx_mem.h is #included although AT25DFX_MEM is disabled
#endif


#include "ctrl_access.h"

//_____ D E C L A R A T I O N S ____________________________________________

/*! \name Control Interface
 */
//! @{

/*! \brief Tests the memory state and initializes the memory if required.
 *
 * The TEST UNIT READY SCSI primary command allows an application client to poll
 * a LUN until it is ready without having to allocate memory for returned data.
 *
 * This command may be used to check the media status of LUNs with removable
 * media.
 *
 * \return Status.
 */
extern Ctrl_status at25dfx_test_unit_ready(void);

/*! \brief Returns the address of the last valid sector in the memory.
 *
 * \param u32_nb_sector Pointer to the address of the last valid sector.
 *
 * \return Status.
 */
extern Ctrl_status at25dfx_read_capacity(U32 *u32_nb_sector);

/*! \brief Returns the write-protection state of the memory.
 *
 * \return \c true if the memory is write-protected, else \c false.
 *
 * \note Only used by removable memories with hardware-specific write
 *       protection.
 */
extern bool at25dfx_wr_protect(void);

/*! \brief Tells whether the memory is removable.
 *
 * \return \c true if the memory is removable, else \c false.
 */
extern bool at25dfx_removal(void);

/*! \brief Unload/load the memory.
 *
 * \param unload \c true to unload, \c false to load.
 * \return \c true if the operation is success, else \c false.
 */
extern bool at25dfx_unload(bool unload);

//! @}


#if ACCESS_MEM_TO_RAM == true

/*! \name MEM <-> RAM Interface
 */
//! @{

/*! \brief Copies 1 data sector from the memory to RAM.
 *
 * \param addr  Address of first memory sector to read.
 * \param ram   Pointer to RAM buffer to write.
 *
 * \return Status.
 */
extern Ctrl_status at25dfx_df_2_ram(U32 addr, void *ram);

/*! \brief Copies 1 data sector from RAM to the memory.
 *
 * \param addr  Address of first memory sector to write.
 * \param ram   Pointer to RAM buffer to read.
 *
 * \return Status.
 */
extern Ctrl_status at25dfx_ram_2_df(U32 addr, const void *ram);

//! @}

#endif

#endif /* AT25DFX_MEM_H_ */
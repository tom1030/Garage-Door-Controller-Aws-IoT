/*
 * conf_sst25vf.h
 *
 * Created: 26/9/2017 下午 3:20:29
 *  Author: NSC
 */ 


#ifndef CONF_SST25VF_H_
#define CONF_SST25VF_H_


#include <board.h>
#include "sst25vf.h"

//! Select the SPI module SST25VF is connected to
#define SST25VF_SPI                 SERIALFLASH_SPI_MODULE

/** SST25VF device type */
#define SST25VF_MEM_TYPE            SST25VF_080B

#define SST25VF_SPI_PINMUX_SETTING  SERIALFLASH_SPI_MUX_SETTING
#define SST25VF_SPI_PINMUX_PAD0     SERIALFLASH_SPI_PINMUX_PAD0
#define SST25VF_SPI_PINMUX_PAD1     SERIALFLASH_SPI_PINMUX_PAD1
#define SST25VF_SPI_PINMUX_PAD2     SERIALFLASH_SPI_PINMUX_PAD2
#define SST25VF_SPI_PINMUX_PAD3     SERIALFLASH_SPI_PINMUX_PAD3

#define SST25VF_CS                  SERIALFLASH_SPI_CS 

//! SPI master speed in Hz.
#define SST25VF_CLOCK_SPEED         12000000  //12M Hz


//! Size of SST25VF data flash memories to manage.
#define SST25VF_MEM_SIZE            20  // 1MB = 1 * 1024 * 2014

//! Number of SST25VF components to manage.
#define SST25VF_MEM_CNT             1


#endif /* CONF_SST25VF_H_ */
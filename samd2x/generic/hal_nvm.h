/*!
 * @file
 * @brief Hardware Abstraction Layer for NVM Flash Memory
 *
 * Author: Vikram Upparpalli
 */

#ifndef HAL_NVM_H
#define HAL_NVM_H

#include "samd21g18a.h"

typedef enum
{
  FLASH_SUCCESS = 0,
  FLASH_ERR_BUSY,         /* Peripheral Busy */
  FLASH_ERR_PGME,         /* Operation failure, Invalid Command */
  FLASH_ERR_LOCKE,        /* Operation failure, Programming atleast one locked region*/
  FLASH_ERR_NVME,         /* Operation failure, NVM Controller Error */
  FLASH_ERR_BYTES,        /* Invalid number of bytes passed */
  FLASH_ERR_ADDRESS,      /* Invalid address or address not on a programming boundary */
  FLASH_ERR_BLOCKS,       /* The "number of blocks" argument is invalid */
  FLASH_ERR_TIMEOUT,      /* Timeout Condition */
} Flash_Error_t;

/*!
 * Macros for Address Resolution
 * Address <- - - -> {Rows, Pages}
 * {Rows, Pages} <- - - -> FlashMemIndex
 */
#define PAGES_PER_ROW                                    (4)
#define BYTES_PER_FLASH_INDEX                            (4)
#define FLASH_MEM                                        ((volatile uint32_t *)FLASH_ADDR)
#define FLASH_MEM_WORDS                                  (FLASH_SIZE/sizeof(uint32_t))
#define MIN_PGM_SIZE                                     (4)
#define ROW_SIZE                                         (FLASH_PAGE_SIZE * PAGES_PER_ROW)
#define ADDR_REG_VAL(address)                            (address >> 1)
#define OFFSET_ADDRESS(address)                          (address % ROW_SIZE)
#define ROW_WITHIN_FLASH(address)                        (address/ROW_SIZE)
#define FORROW()                                         for(uint16_t iteration = 0; \
                                                             iteration <= (ROW_SIZE/BYTES_PER_FLASH_INDEX); \
                                                             iteration++)
#define DELAY                                            (20000)
typedef struct{
	  // Pointer to the sourceData
	  uint8_t *currentPageBuffer;
	  /* Offset from start of Page from where the
	   * data needs to be written
	   */
	  uint8_t offsetFreeByteLocation;
	  /*
	   * Number of bytes to be written in current iteration
	   */
	  uint8_t numberOfBytesCurrentPage;
	  /*
	   * This offset is applied to sourceData depending
	   *  on how many bytes are written
	   */
	  uint32_t bufferOffset;
	} CurrentPageInfo_t;

/*!
 * Macros for Address Resolution
 * FlashMemIndex <- - - -> {Rows, Pages}
 * {Rows, Pages} <- - - -> Address
 */
#define FLASH_ADDRESS(rowNumber, pageOffset)        (((rowNumber * ROW_SIZE) + pageOffset)/BYTES_PER_FLASH_INDEX)
#define FLASH_ROW_ADDRESS(rowNumber)                     ((rowNumber * ROW_SIZE)/BYTES_PER_FLASH_INDEX)

Flash_Error_t SamD2xxFlashWrite(uint32_t sourceAddress, uint32_t destinationAddress, uint32_t numberOfBytes);
Flash_Error_t SamD2xxFlashErase(uint32_t rowStartAddress, uint32_t numberOfRows);
Flash_Error_t SamD2xxFlashBlankCheck(uint32_t rowAddress, uint32_t numberOfBytes);
void NvmInit(void);

#endif /* HAL_NVM_H */

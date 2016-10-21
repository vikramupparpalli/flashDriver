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
    e_Nvm_Busy,
    e_Nvm_Ready,
    e_Nvm_Invalid_Address,
	e_Nvm_PageNotEmpty,
	e_Nvm_PageBufferComplete,
	e_Nvm_PageBufferIncomplete,
	e_Nvm_CurrentRowErased,
	e_Nvm_Write_Rejected
} NvmStatus_e;

/*!
 * Macros for Address Resolution
 * Address <- - - -> {Rows, Pages}
 * {Rows, Pages} <- - - -> FlashMemIndex
 */
#define PAGES_PER_ROW                                    (4)
#define BYTES_PER_FLASH_INDEX                            (4)
#define FLASH_MEM                                        ((volatile uint32_t *)FLASH_ADDR)
#define FLASH_MEM_WORDS                                  (FLASH_SIZE/sizeof(uint32_t))
#define PAGE_WITHIN_ROW(address)                         (address % (FLASH_PAGE_SIZE*PAGES_PER_ROW))
#define ROW_WITHIN_FLASH(address)                        (address/(FLASH_PAGE_SIZE*PAGES_PER_ROW))
#define FORPAGE(value)                                   for(uint16_t value = 0; value <= (FLASH_PAGE_SIZE/BYTES_PER_FLASH_INDEX); value++)

typedef struct{
	   uint8_t     rowData[(FLASH_PAGE_SIZE * PAGES_PER_ROW)];
	   uint8_t     *currentPageBuffer;
	   uint8_t     offsetFreeLoc;
	   uint16_t    currentRowNumber;
	   uint8_t     currentPagePosition;
	   NvmStatus_e currentPageStatus;
	} currentRowBuffer_t;

/*!
 * Macros for Address Resolution
 * FlashMemIndex <- - - -> {Rows, Pages}
 * {Rows, Pages} <- - - -> Address
 */
#define FLASH_PAGE_ADDRESS(rowNumber, pageOffset)        (((rowNumber * PAGES_PER_ROW * FLASH_PAGE_SIZE) + pageOffset)/BYTES_PER_FLASH_INDEX)
#define FLASH_ROW_ADDRESS(rowNumber)                     ((rowNumber * PAGES_PER_ROW * FLASH_PAGE_SIZE)/BYTES_PER_FLASH_INDEX)

NvmStatus_e NvmRowErase(const uint32_t address);
NvmStatus_e SamD2xxFlashWrite(const uint32_t destinationAddress, uint32_t sourceAddress, uint16_t numberOfBytes);
void NvmInit(void);

#endif /* HAL_NVM_H */

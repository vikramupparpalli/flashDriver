/*!
 * @file
 * @brief Hardware Abstraction Layer for NVM Flash Memory
 *
 * Author: Vikram Upparpalli
 */

#include "hal_nvm.h"

/*TODO: An alternative exit path for if we are past 6ms timer*/
#define NVM_READY       (NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY)
#define WAIT_FOR_NVM()  while(!NVM_READY) { \
	                          uint32_t nvmDelay=DELAY; \
							  while(nvmDelay) \
							       { nvmDelay--; } \
							  NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK; \
						}
#define CLEAR           0xFFFFFFFF

static CurrentPageInfo_t currentWrite;
/************************************************************************/
/*                      Address Resolution                              */
/************************************************************************/

uint32_t getPageAddress(uint32_t destinationAddress)
{
	uint16_t pageNumber   = 0;
	uint32_t pageAddress;
	uint8_t  pagePosinRow = (OFFSET_ADDRESS(destinationAddress)/FLASH_PAGE_SIZE);
	pageNumber            = (ROW_WITHIN_FLASH(destinationAddress) * PAGES_PER_ROW) + pagePosinRow;
	pageAddress           = pageNumber * FLASH_PAGE_SIZE;
	return(pageAddress);
}

uint32_t getFlashMemIndex(uint32_t address)
{
	return(FLASH_ADDRESS(ROW_WITHIN_FLASH(address), OFFSET_ADDRESS(address)));
}


Flash_Error_t IsRowEmpty(uint32_t pageAddress)
{
	Flash_Error_t status = FLASH_SUCCESS;
	WAIT_FOR_NVM()
	uint16_t nvmMemory = getFlashMemIndex(pageAddress);
	FORROW()
	{
		if(CLEAR != FLASH_MEM[nvmMemory++])
		{
			//status = e_Nvm_RowNotEmpty;
			break;
		}
	}
	return(status);
}

/************************************************************************/
/*                      NVM Routines                                    */
/************************************************************************/

void NvmInit(void)
{
	PM->APBBMASK.reg    |= PM_APBBMASK_NVMCTRL;

	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;

	NVMCTRL->CTRLB.bit.MANW = 0;
}

static void NvmRowErase(uint32_t address)
{
	WAIT_FOR_NVM() /*Dead While loop*/
	NVMCTRL->ADDR.reg    = ADDR_REG_VAL(address);

	NVMCTRL->CTRLA.reg   = NVMCTRL_CTRLA_CMD_ER | NVMCTRL_CTRLA_CMDEX_KEY;
}

static void NvmPageWrite(uint32_t address, uint8_t *pageBuffer, uint8_t numberOfBytes)
{
	uint32_t *dataBuffer = (uint32_t *)pageBuffer;
	WAIT_FOR_NVM()
	NVMCTRL->CTRLA.reg   = NVMCTRL_CTRLA_CMD_PBC | NVMCTRL_CTRLA_CMDEX_KEY;
	uint16_t nvmMemory = getFlashMemIndex(address);
	for(volatile uint16_t iteration = 0; iteration < (FLASH_PAGE_SIZE/BYTES_PER_FLASH_INDEX); iteration++)
	{
		/*
		 * This runs for 16 times(FLASH_PAGE_SIZE/BYTES_PER_FLASH_INDEX)
		 * 64/4 = 16.
		 * Covers 3 cases of PageWrite Situation
		 * a. Full Page Write
		 * b. Page Write from middle of the page to end of the page
		 * c. Page Write from start of the page to middle of the page
		 */
		if(iteration > (FLASH_PAGE_SIZE - currentWrite.offsetFreeByteLocation)/BYTES_PER_FLASH_INDEX)
		{
			/*!
			 *  We get here when we have written a portion of the bytes
			 *  for current Write Request and have reached end of the page.
			 */
			break;
		}
		else if(iteration > ((numberOfBytes-1)/BYTES_PER_FLASH_INDEX))
		{
			/* Stuff rest of the page with 0xFF for each byte */
			FLASH_MEM[nvmMemory++] = CLEAR;
		}
		else
		{
			/* We get here when we have data and there is space left in the current page*/
			FLASH_MEM[nvmMemory++] = dataBuffer[iteration + currentWrite.bufferOffset];
		}
	}
}

static Flash_Error_t NvmErrorStatus(void)
{
	Flash_Error_t status = FLASH_SUCCESS;
	if((REG_NVMCTRL_STATUS & NVMCTRL_STATUS_PROGE))
	{
		status = FLASH_ERR_PGME;
	}
	else if((REG_NVMCTRL_STATUS & NVMCTRL_STATUS_LOCKE))
	{
		status = FLASH_ERR_LOCKE;
	}
	else if((REG_NVMCTRL_STATUS & NVMCTRL_STATUS_NVME))
	{
		status = FLASH_ERR_NVME;
	}
	return(status);
}

/************************************************************************/
/*                       Helper Functions                               */
/************************************************************************/
uint32_t UpdatePageHandle(uint32_t sourceAddress, uint32_t destinationAddress, uint32_t numberOfBytes)
{
	/*!
	 * This function is called when Write was issued and is not complete yet.
	 * Inputs: Source and destination for current page write
	 *         and Number of bytes yet to be written
	 * Output: Updates the offset addresses and returns number of bytes scheduled
	 *         for current iteration
	 */
	currentWrite.currentPageBuffer      = sourceAddress;
	uint32_t pageAddress                = getPageAddress(destinationAddress);
	currentWrite.offsetFreeByteLocation = destinationAddress - pageAddress;

	if(numberOfBytes > (FLASH_PAGE_SIZE - currentWrite.offsetFreeByteLocation))
	{
		/* We end up here when a portion of the sourceData left to be written goes into current Page*/
		currentWrite.numberOfBytesCurrentPage = (FLASH_PAGE_SIZE - currentWrite.offsetFreeByteLocation);
	}
	else
	{
		/* We reach here when last part of the sourceData can fit into currentPage*/
		currentWrite.numberOfBytesCurrentPage = numberOfBytes;
	}
	return(numberOfBytes - currentWrite.numberOfBytesCurrentPage);
}

Flash_Error_t DataSizeTests(uint32_t destinationAddress, uint32_t numberOfBytes)
{
	if((((numberOfBytes - 1) + destinationAddress) > (uint32_t)FLASH_SIZE) |
	   (numberOfBytes < MIN_PGM_SIZE)                                      |
	   (destinationAddress < FLASH_ADDR)                                   |
	   (numberOfBytes > (FLASH_SIZE - 1))                                  |
	   (numberOfBytes == 0)                                                )
	{
		return(FLASH_ERR_BYTES);
	}
	else
	{
		return(FLASH_SUCCESS);
	}
}
/************************************************************************/
/*                        FLASH APIs                                    */
/************************************************************************/
Flash_Error_t SamD2xxFlashWrite(uint32_t sourceAddress, uint32_t destinationAddress, uint32_t numberOfBytes)
{
	uint32_t numberOfBytesLeft = 0;
	if(FLASH_ERR_BYTES == DataSizeTests(destinationAddress, numberOfBytes))
	{
		/*!
		 * Checks for validity of numberOfBytes, Destination Address etc
		 */
		return(FLASH_ERR_BYTES);
	}
	else
	{
		numberOfBytesLeft = numberOfBytes;
	}
	while(numberOfBytesLeft)
	{
		/*!
		 * The following code manages the Nvm Write for entire chunk
		 */
		numberOfBytesLeft  = UpdatePageHandle(sourceAddress, \
				                              destinationAddress, \
											  numberOfBytesLeft);

		NvmPageWrite(destinationAddress, currentWrite.currentPageBuffer, \
					 currentWrite.numberOfBytesCurrentPage);

		/* Prepare for next iteration */
		destinationAddress +=  currentWrite.numberOfBytesCurrentPage;
		if(numberOfBytesLeft == 0)
		{
			currentWrite.currentPageBuffer        = 0x00;
			currentWrite.numberOfBytesCurrentPage = 0;
			currentWrite.offsetFreeByteLocation   = 0;
			currentWrite.bufferOffset             = 0;
		}
		else
		{
			currentWrite.bufferOffset            += ((currentWrite.numberOfBytesCurrentPage + 1)/ \
										                          BYTES_PER_FLASH_INDEX);
		}
	}
	return(NvmErrorStatus());
}

/* Checking the error status at the end because the Errors are not cleared until
   an explicit  "1" is written to Error Bits*/
Flash_Error_t SamD2xxFlashErase(uint32_t rowStartAddress, uint32_t numberOfRows)
{
	for(uint16_t iteration=0; iteration < (uint16_t)numberOfRows; iteration++, rowStartAddress += ROW_SIZE)
	{
		NvmRowErase(rowStartAddress);
	}
	return(NvmErrorStatus());
}

Flash_Error_t SamD2xxFlashBlankCheck(uint32_t address, uint32_t numberOfBytes)
{
  Flash_Error_t status;
  if(numberOfBytes <= ROW_SIZE)
  {
    uint32_t rowAddress = (ROW_WITHIN_FLASH(address) * ROW_SIZE);
    status              = IsRowEmpty(rowAddress);
		/*if(e_Nvm_RowNotEmpty != status)
		{
			status = FLASH_SUCCESS;
		}*/
	}
	return(status);
}

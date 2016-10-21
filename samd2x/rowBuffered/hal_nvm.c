/*!
 * @file
 * @brief Hardware Abstraction Layer for NVM Flash Memory
 *
 * Author : Vikram Upparpalli
 */

#include "hal_nvm.h"
#include "string.h"

#define NVM_READY       (NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY)
#define WAIT_FOR_NVM()  while(!NVM_READY) ;
#define CLEAR           0xFFFFFFFF

currentRowBuffer_t flashMemHandle;

/************************************************************************/
/*                      Address Resolution                              */
/************************************************************************/

uint32_t getPageAddress(uint32_t destinationAddress)
{
	uint16_t pageNumber = 0;
	uint32_t pageAddress;
	uint8_t  pagePosinRow = (PAGE_WITHIN_ROW(destinationAddress)/FLASH_PAGE_SIZE);
	pageNumber = (ROW_WITHIN_FLASH(destinationAddress) * PAGES_PER_ROW) + pagePosinRow;
	pageAddress = pageNumber * FLASH_PAGE_SIZE;
	return(pageAddress);
}

uint32_t getFlashMemIndex(uint32_t address)
{
	return(FLASH_PAGE_ADDRESS(ROW_WITHIN_FLASH(address), PAGE_WITHIN_ROW(address)));
}


NvmStatus_e IsPageEmpty(uint32_t pageAddress)
{
	NvmStatus_e status = e_Nvm_Ready;
	WAIT_FOR_NVM()
	uint16_t nvmMemory = getFlashMemIndex(pageAddress);
	FORPAGE(iteration)
	{
		if(CLEAR != FLASH_MEM[nvmMemory++])
		{
			status = e_Nvm_PageNotEmpty;
			break;
		}
	}
	return(status);
}
/************************************************************************/
/*                      Flash Memory Row Buffer                         */
/************************************************************************/

void InitializeFlashMemHandle(void)
{
	flashMemHandle.currentRowNumber = ROW_WITHIN_FLASH(0x00000000);
	flashMemHandle.currentPagePosition = (PAGE_WITHIN_ROW(0x00000000)/FLASH_PAGE_SIZE);
	flashMemHandle.currentPageBuffer = &flashMemHandle.rowData[(FLASH_PAGE_SIZE * flashMemHandle.currentPagePosition)];
}

uint32_t UpdateCurrentBuffer(uint32_t destinationAddress)
{
	uint32_t pageAddress = getPageAddress(destinationAddress);
	flashMemHandle.offsetFreeLoc = (destinationAddress - pageAddress);
	flashMemHandle.currentRowNumber = ROW_WITHIN_FLASH(destinationAddress);
	flashMemHandle.currentPagePosition = (PAGE_WITHIN_ROW(destinationAddress)/FLASH_PAGE_SIZE);
	flashMemHandle.currentPageBuffer = &flashMemHandle.rowData[(FLASH_PAGE_SIZE * flashMemHandle.currentPagePosition)];
	return(pageAddress);
}

NvmStatus_e FlushRowBuffer(void)
{
	memset(flashMemHandle.rowData,0xFF,(FLASH_PAGE_SIZE * PAGES_PER_ROW));
	return(e_Nvm_Ready);
}

/************************************************************************/
/*                      NVM Routines                                    */
/************************************************************************/
void NvmInit(void)
{
	PM->APBBMASK.reg |= PM_APBBMASK_NVMCTRL;

	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;

	NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_SLEEPPRM(NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS_Val) |
	                    (0x00 << NVMCTRL_CTRLB_MANW_Pos) |
						NVMCTRL_CTRLB_RWS(NVMCTRL->CTRLB.bit.RWS) |
						(0x00 << NVMCTRL_CTRLB_CACHEDIS_Pos) |
						NVMCTRL_CTRLB_READMODE(0);

	InitializeFlashMemHandle();
	FlushRowBuffer();
}

NvmStatus_e NvmRowErase(uint32_t address)
{
	WAIT_FOR_NVM()
	NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_PBC | NVMCTRL_CTRLA_CMDEX_KEY;
	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
	
	NVMCTRL->ADDR.reg  = (uintptr_t)&FLASH_MEM[getFlashMemIndex(address)];

	NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_ER | NVMCTRL_CTRLA_CMDEX_KEY;
	    
    WAIT_FOR_NVM()
	return e_Nvm_Ready;
}

NvmStatus_e NvmFullPageWrite(uint32_t address, uint8_t *pageBuffer)
{
	NvmStatus_e status;
	uint32_t *dataBuffer = (uint32_t *)pageBuffer;
	status = IsPageEmpty(address);
	if(e_Nvm_PageNotEmpty != status)
	{
		WAIT_FOR_NVM()
		uint16_t nvmMemory = getFlashMemIndex(address);
		NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_PBC | NVMCTRL_CTRLA_CMDEX_KEY;
		NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
		for(uint16_t iteration = 0; iteration <= (FLASH_PAGE_SIZE/BYTES_PER_FLASH_INDEX); iteration++)
		{
			FLASH_MEM[nvmMemory++] = dataBuffer[iteration];
		}
		WAIT_FOR_NVM()
		
		status = e_Nvm_Ready;
	}
	
	return status;
}

NvmStatus_e WriteManager(uint32_t destinationAddress, uint8_t *sourceAddress, uint16_t numberOfBytes)
{
	NvmStatus_e status = e_Nvm_Ready;
	uint8_t  currentPageOffset = flashMemHandle.offsetFreeLoc;
	for(uint8_t iteration=0; iteration < numberOfBytes; iteration++)
	{
		flashMemHandle.currentPageBuffer[currentPageOffset++] = sourceAddress[iteration];
		if((iteration % FLASH_PAGE_SIZE) == (FLASH_PAGE_SIZE - flashMemHandle.offsetFreeLoc) - 1)
		{
			flashMemHandle.currentPageBuffer = &flashMemHandle.rowData[(FLASH_PAGE_SIZE * flashMemHandle.currentPagePosition)];
			status = NvmFullPageWrite(destinationAddress, flashMemHandle.currentPageBuffer);
			if(e_Nvm_PageNotEmpty != status)
			{
				flashMemHandle.offsetFreeLoc = 0;
				currentPageOffset = flashMemHandle.offsetFreeLoc;
				if(flashMemHandle.currentPagePosition < (PAGES_PER_ROW - 1))
				{
					flashMemHandle.currentPagePosition++;
				}
				else
				{
					flashMemHandle.currentPagePosition = 0;
					flashMemHandle.currentRowNumber++;
					FlushRowBuffer();
				}
				flashMemHandle.currentPageBuffer = &flashMemHandle.rowData[(FLASH_PAGE_SIZE * flashMemHandle.currentPagePosition)];
				destinationAddress = destinationAddress + FLASH_PAGE_SIZE;
			}
			else
			{
				status = NvmRowErase(destinationAddress);
			}
		}
	}
	return(status);
}

/************************************************************************/
/*                          Flash APIs                                  */
/************************************************************************/
NvmStatus_e SamD2xxFlashWrite(uint32_t destinationAddress, uint32_t sourceAddress, uint16_t numberOfBytes)
{
	NvmStatus_e status = e_Nvm_Ready;
	uint32_t pageAddress = UpdateCurrentBuffer(destinationAddress);
	status = WriteManager(pageAddress, (uint8_t *)sourceAddress, numberOfBytes);
	if(e_Nvm_CurrentRowErased == status)
	{
		/*Reflash the entire row*/
	}
	return(status);
}

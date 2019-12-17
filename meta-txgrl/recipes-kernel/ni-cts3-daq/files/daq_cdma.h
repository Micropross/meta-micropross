/******************************************************************************
 *
 *   Copyright (C) 2019  National Instruments Corporation. All rights reserved.
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *****************************************************************************/

#ifndef PCIECDMA_H_
#define PCIECDMA_H_

#include <linux/spinlock_types.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>

#define CDMA_REG_CDMACR	 0x00    /* CDMA control register offset */
#define CDMA_REG_CDMASR	 0x04    /* CDMA Status Register */
#define CDMA_REG_CURDESC_PNTR   0x08    /* Current Descriptor Pointer Register */
#define CDMA_REG_TAILDESC_PNTR  0x10    /* Tail Descriptor Pointer Register */
#define CDMA_REG_SA	     0x18    /* Source Address Register */
#define CDMA_REG_DA	     0x20    /* Destination Address Register */
#define CDMA_REG_BTT	    0x28    /* bytes to Transfer Register */

#define CDMA_MASK_CDMASR_ERR_IRQ	(1 << 14)
#define CDMA_MASK_CDMASR_IOC_IRQ	(1 << 12)
#define CDMA_MASK_CDMASR_DMADECERR      (1 << 6)
#define CDMA_MASK_CDMASR_DMASLVERR      (1 << 4)
#define CDMA_MASK_CDMASR_DMAINTERR      (1 << 4)
#define CDMA_MASK_CDMASR_IDLE	   (1 << 1)

#define CDMA_MASK_CDMACR_ERR_IRQEN      (1 << 14)
#define CDMA_MASK_CDMACR_IOC_IRQEN      (1 << 12)
#define CDMA_MASK_CDMACR_RESET	  (1 << 2)

#define CDMA_DDRAXIADDR		 0xC0000000
#define CDMA_AXIBAR0AXIADDR	     0x80000000

/**
 * @enum PCIE_DMA_STATUS
 * Status of the dma transfer
 */
enum PCIE_DMA_STATUS
{
	PCIE_DMA_SUCCEED, /**< DMA transfer succeed */
	PCIE_DMA_ERROR,   /**< DMA transfer generic error */
	PCIE_DMA_FREE,    /**< DMA free */
	PCIE_DMA_BUSY,    /**< DMA busy */
};

/*#define SPEED_MEASUREMENT */ /**< Enable speed measurement */

/**
 * @struct PCIE_dma
 * This structure contains data about a DMA resource
 */
struct PCIE_dma
{
	struct spinlock lock;		/**< Used for avoiding concurrent access to the dma data */
	struct device  * pDev;
	wait_queue_head_t wait;	 /**< Device work queue */
	enum PCIE_DMA_STATUS status;    /**< DMA status */
	uint32_t CDMABaseAddr;
	int dir;			/**< DMA direction @see PCIE_DMA_STATUS */
#ifdef SPEED_MEASUREMENT
	struct timeval start_timestamp;
	struct timeval end_timestamp;
#endif
};

struct PCIE_cma_device
{
	size_t size;			/* CMA buffer size */
	dma_addr_t phys_addr;	/* CMA buffer physical start address*/
	void *virt_addr;		/* CMA buffer virtual start address*/
};

irqreturn_t PCI_CDMAIRQHandler(int irq, struct PCIE_dma * dma);
int PCI_CDMAInit(struct device * pDev, struct PCIE_dma * dma, uint32_t CDMABaseAddr);
int PCI_CDMAExit(struct PCIE_dma * dma);
ssize_t PCI_CDMAWrite(struct PCIE_dma * dma, struct PCIE_cma_device *cma_dev, size_t count, loff_t * f_pos);
ssize_t PCI_CDMARead(struct PCIE_dma *pDma, struct PCIE_cma_device *cma_dev, size_t count, loff_t *ppos);
int PCI_AllocDMA(struct device *dev, struct PCIE_cma_device *cma, ssize_t size);
void PCI_FreeDMA(struct device *dev, struct PCIE_cma_device *cma);

#endif /* PCIECDMA_H_ */

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

#include <linux/device.h>
#include <linux/sched.h>
#include <linux/fs.h>

#include "daq_cdma.h"

//#define VERBOSE
#ifdef VERBOSE
#define DPRINT(a...) pr_info("ni_cts3_daq: "); pr_cont(a); pr_cont(" (%s:%d)\n", __func__, __LINE__);
#else
#define DPRINT(a...)
#endif

inline static u32 PCIE_CDMAReadReg(struct PCIE_dma * dma, u32 reg)
{
	u32 val = readl((void*)(dma->CDMABaseAddr + reg));
	DPRINT("CDMA R 0x%08x : 0x%08x", (unsigned int)(dma->CDMABaseAddr + reg), val);
	return val;
}

inline static void PCIE_CDMAWriteReg(struct PCIE_dma * dma, u32 reg, u32 val)
{
	DPRINT("CDMA W 0x%08x : 0x%08x", (unsigned int)(dma->CDMABaseAddr + reg), val);
	writel(val, (void *)(dma->CDMABaseAddr + reg));
}

static int PCIE_DMAEvent(struct PCIE_dma * dma)
{
	u32 src;
	u32 dest;
	u32 val;
	u32 count;
	int ret = 0;

	DPRINT("Enter");

	/* 7. Determine the interrupt source */
	val = PCIE_CDMAReadReg(dma, CDMA_REG_CDMASR);

	if ((val & CDMA_MASK_CDMASR_IOC_IRQ) == CDMA_MASK_CDMASR_IOC_IRQ)
	{
		/* 8. Clear the CDMASR.IOC_Irq bit by writing 1 to
		 *  DMASR.IOC_Irq bit position*/
		PCIE_CDMAWriteReg(dma, CDMA_REG_CDMASR, val |
				  CDMA_MASK_CDMASR_IOC_IRQ |
				  CDMA_MASK_CDMASR_DMAINTERR |
				  CDMA_MASK_CDMASR_DMADECERR);

		/* Read BTT acknowledge the interrupt */
		count = PCIE_CDMAReadReg(dma, CDMA_REG_BTT);

		/* 9. Ready for another transfer */
		if ((val & CDMA_MASK_CDMASR_ERR_IRQ) ||
		    (val & CDMA_MASK_CDMASR_DMAINTERR) ||
		    (val & CDMA_MASK_CDMASR_DMASLVERR) ||
		    (val & CDMA_MASK_CDMASR_DMADECERR))
		{
			/* Transfer failed */
			printk(KERN_ERR "DMA irq : Transfer failed CDMASR 0x%08X "
					"CDMASA 0x%08X CDMADA 0x%08X CDMABTT 0x%08X\n",
					val, PCIE_CDMAReadReg(dma, CDMA_REG_SA),
					PCIE_CDMAReadReg(dma, CDMA_REG_DA),
					PCIE_CDMAReadReg(dma, CDMA_REG_BTT));

			dma->status = PCIE_DMA_ERROR;

			/* reset the CDMA */
			PCIE_CDMAWriteReg(dma, CDMA_REG_CDMACR,
					  CDMA_MASK_CDMACR_RESET);
		}
		else
		{
			DPRINT("DMA irq : Transfer succeed");
			dma->status = PCIE_DMA_SUCCEED;

			src = PCIE_CDMAReadReg(dma, CDMA_REG_SA);
			dest = PCIE_CDMAReadReg(dma, CDMA_REG_DA);

			DPRINT("CDMA_REG_SA val 0x%08X", src);
			DPRINT("CDMA_REG_DA val 0x%08X", dest);
			if (src == CDMA_AXIBAR0AXIADDR)
			{
				/* Write done */
				DPRINT("DMA irq :write done");
			}
			else if (dest == CDMA_AXIBAR0AXIADDR)
			{
				/* Read done */
				DPRINT("DMA irq :write done");
			}
			else
			{
				printk(KERN_ERR "Is it a read or write event ????\n");
			}
		}
		/* Notify that DMA transfer is done */
		wake_up(&dma->wait);
	}
	else
	{
		printk(KERN_ERR "Not a valid DMA IRQ\n");
		ret = -EAGAIN;
	}

	DPRINT("Leave");
	return ret;
}


irqreturn_t PCI_CDMAIRQHandler(int irq, struct PCIE_dma * dma)
{
	u32 cdma_isr;
	int dma_done = 0;
	irqreturn_t ret = IRQ_HANDLED;
	unsigned long flags;

	DPRINT("Enter");

	spin_lock_irqsave(&dma->lock, flags);

	PCIE_CDMAWriteReg(dma, CDMA_REG_CDMACR, 0);

	cdma_isr = PCIE_CDMAReadReg(dma, CDMA_REG_CDMASR);
	if ((cdma_isr & CDMA_MASK_CDMASR_IOC_IRQ) ==
			CDMA_MASK_CDMASR_IOC_IRQ)
	{
		if (dma->status == PCIE_DMA_BUSY &&
				!PCIE_DMAEvent(dma))
		{
			dma_done = 1;
		}
	}

	if (dma_done == 0)
	{
		u32 val;

		/* Get status register  */
		val = PCIE_CDMAReadReg(dma, CDMA_REG_CDMASR);
		/* 8. Clear the CDMASR.IOC_Irq bit by writing 1 to
		 *  DMASR.IOC_Irq bit position*/
		PCIE_CDMAWriteReg(dma, CDMA_REG_CDMASR, val |
				CDMA_MASK_CDMASR_IOC_IRQ |
				CDMA_MASK_CDMASR_DMAINTERR |
				CDMA_MASK_CDMASR_DMADECERR);

		/* Read BTT acknowledge the interrupt */
		PCIE_CDMAReadReg(dma, CDMA_REG_BTT);

		printk(KERN_ERR
				"%s Unattended CDMA interrupt received done %d\n",
				__func__, dma_done);
		ret = IRQ_NONE;
	}

	/* Enable Irq */
	PCIE_CDMAWriteReg(dma, CDMA_REG_CDMACR,
			CDMA_MASK_CDMACR_IOC_IRQEN |
			CDMA_MASK_CDMACR_ERR_IRQEN);

	spin_unlock_irqrestore(&dma->lock, flags);

	DPRINT("Leave");
	return ret;
}

int PCI_AllocDMA(struct device *dev, struct PCIE_cma_device *cma, ssize_t size)
{
	int ret = 0;

	DPRINT("Allocate DMA buffer");

	/* Allocate buffer for DMA transfer */
	cma->virt_addr = dma_alloc_coherent(dev, size, &cma->phys_addr, GFP_KERNEL);
	if (!cma->virt_addr)
	{
		cma->size = 0;
		printk(KERN_ERR "DMA buffer allocation failed size 0x%08X\n",
			size);
		ret = -ENOMEM;
	}
	else
	{
		cma->size = size;
	}

	DPRINT("DMA Buffer Allocation: 0x%08X->0x%08X",
		(unsigned int)cma->virt_addr, (unsigned int)cma->phys_addr);
	return ret;
}

void PCI_FreeDMA(struct device *dev, struct PCIE_cma_device *cma)
{
	DPRINT("Free DMA buffer");

	/* Free buffer */
	if (cma->phys_addr)
	{
		dma_free_coherent(dev, cma->size, cma->virt_addr, cma->phys_addr);
	}
}

static int PCIE_Transfer(struct PCIE_dma * pDma, int write,
		size_t count, loff_t * f_pos)
{
	u32 val;
	u32 src, dest;

	DPRINT("Enter");
	if (count == 0)
	{
		DPRINT("Count = 0");
		return -EINVAL;
	}

	/* 1. Verify CDMASR.IDLE = 1
	   Check that there is no transfer running */
	val = PCIE_CDMAReadReg(pDma, CDMA_REG_CDMASR);
	if (!(val & (CDMA_MASK_CDMASR_IDLE | CDMA_MASK_CDMASR_IOC_IRQ)))
	{
		DPRINT("%s DMA transfer already running", __func__);
		return -EBUSY;
	}

	/* 2. Program the CDMACR.IOC_IrqEn and CDMACR.ERR_IrqEn => Enable Irq */
	PCIE_CDMAWriteReg(pDma, CDMA_REG_CDMACR,
			  CDMA_MASK_CDMACR_IOC_IRQEN |
			  CDMA_MASK_CDMACR_ERR_IRQEN);

	if (write)
	{
		src = CDMA_AXIBAR0AXIADDR;
		dest = (u32) (*f_pos + CDMA_DDRAXIADDR);
	}
	else
	{
	       src = (u32) (*f_pos + CDMA_DDRAXIADDR);
	       dest = CDMA_AXIBAR0AXIADDR;
	}

	/* 3. Write the source address */
	PCIE_CDMAWriteReg(pDma, CDMA_REG_SA, src);

	/* 4. Write the transfer destination address */
	PCIE_CDMAWriteReg(pDma, CDMA_REG_DA, dest);

	pDma->status = PCIE_DMA_BUSY;

	if (write)
	{
		DPRINT("Start DMA transfer UC => FPGA");
	}
	else
	{
		DPRINT("Start DMA transfer FPGA => UC");
	}

	/* 5. Write the number of bytes and start the transfer */
	PCIE_CDMAWriteReg(pDma, CDMA_REG_BTT, count);

	return 0;
}

int PCI_CDMAInit(struct device * pDev,
		struct PCIE_dma * pDma,
		uint32_t CDMABaseAddr)
{
	spin_lock_init(&pDma->lock);
	pDma->status = PCIE_DMA_FREE;
	pDma->pDev = pDev;
	pDma->CDMABaseAddr = CDMABaseAddr;

	DPRINT("CDMA base address 0x%08X", CDMABaseAddr);

	init_waitqueue_head(&pDma->wait);

	return 0;
}

int PCI_CDMAExit(struct PCIE_dma * dma)
{
	return 0;
}


ssize_t PCI_CDMARead(struct PCIE_dma *pDma, struct PCIE_cma_device *cma_dev, size_t count, loff_t *ppos)
{
	ssize_t Ret = 0;
	unsigned long flags;
	u32 src, dest, size;

	DPRINT("Enter");
	if(count > cma_dev->size)
		count = cma_dev->size;

	/* Lock DMA */
	spin_lock_irqsave(&pDma->lock, flags);

	Ret = PCIE_Transfer(pDma, 0 /* Read */, count, ppos);
	if (!Ret)
	{
		DPRINT("Wait CDMA irq");
		spin_unlock_irqrestore(&pDma->lock, flags);

		Ret = wait_event_timeout(pDma->wait,
				pDma->status != PCIE_DMA_BUSY, HZ*2);
		spin_lock_irqsave(&pDma->lock, flags);
		if (!Ret)
		{
			src = PCIE_CDMAReadReg(pDma, CDMA_REG_SA);
			dest = PCIE_CDMAReadReg(pDma, CDMA_REG_DA);
			size = PCIE_CDMAReadReg(pDma, CDMA_REG_BTT);

			pr_err("ni_cts3_daq: Transfer timeout (SRC = %#x, DEST = %#x, SIZE = %#x)\n", src, dest, size);

			/* reset the CDMA */
			PCIE_CDMAWriteReg(pDma, CDMA_REG_CDMACR,
					  CDMA_MASK_CDMACR_RESET);
			pDma->status = PCIE_DMA_FREE;
			Ret = -EBUSY;
		}
		else if(pDma->status == PCIE_DMA_SUCCEED)
		{
			DPRINT("DMA transfer succeed count = %u", count);
			Ret = count;
			pDma->status = PCIE_DMA_FREE;
		}
		else
		{
			DPRINT("DMA transfer failed");
			/* reset the CDMA */
			PCIE_CDMAWriteReg(pDma, CDMA_REG_CDMACR,
					  CDMA_MASK_CDMACR_RESET);
			pDma->status = PCIE_DMA_FREE;
			Ret = -EINVAL;
		}
	}

	spin_unlock_irqrestore(&pDma->lock, flags);
	return Ret;
}

ssize_t PCI_CDMAWrite(struct PCIE_dma * dma, struct PCIE_cma_device *cma_dev, size_t count, loff_t * f_pos)
{
	ssize_t ret = 0;
	unsigned long flags;

	DPRINT("Enter");

	/* Lock DMA */
	spin_lock_irqsave(&dma->lock, flags);

	/* DMA buffer size is max count for one transfer */
	if(count > cma_dev->size)
		count = cma_dev->size;

	ret = PCIE_Transfer(dma, 1 /* Write */, count, f_pos);
	if (!ret)
	{
		spin_unlock_irqrestore(&dma->lock, flags);
		ret = wait_event_timeout(dma->wait,
				dma->status != PCIE_DMA_BUSY, HZ*2);
		spin_lock_irqsave(&dma->lock, flags);
		if (!ret)
		{
			pr_err("ni_cts3_daq: Write transfer timeout dma.status\n");
			dma->status = PCIE_DMA_FREE;
			ret = -EBUSY;
		}
		else
		{
			DPRINT("DMA transfer succeed count = %u", count);
			ret = count;
		}
	}

	spin_unlock_irqrestore(&dma->lock, flags);

	return ret;
}


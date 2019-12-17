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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pci.h>

#include "daq_pcie.h"
#include "daq_cdma.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Guillaume Fouilleul <guillaume.fouilleul@ni.com>");
MODULE_DESCRIPTION("NI CTS3 DAQ device driver");

#define DDR_SIZE 0x40000000	// 1 GiB

#define PCIE_DRIVER_MAJOR 240
#define PCIE_PCI_DRIVER_NAME	"ni_cts3_daq"

#define PCIE_NB_BAR				3
#define PCIE_DDR_BAR			0
#define PCIE_REG0_BAR			1
#define PCIE_REG1_BAR			2

#define PCI_VENDOR_ID_XILINX	0x10ee
#define PCI_DEVICE_ID_XILINX	0x7024

#define INTC_REG_ISR			0x00
#define INTC_REG_IPR			0x04
#define INTC_REG_IER			0x08
#define INTC_REG_IAR			0x0C
#define INTC_REG_SIE			0x10
#define INTC_REG_CIE			0x14
#define INTC_REG_IVR			0x18
#define INTC_REG_MER			0x1C
#define INTC_REG_IMR			0x20

#define INTC_MASTER_ENABLE_MASK	 	0x00000001
#define INTC_HARDWARE_ENABLE_MASK	0x00000002

// Address translation
#define AXIBAR2PCIEBAR_0U		0x208
#define AXIBAR2PCIEBAR_0L		0x20C

#define INTC_CDMA_MASK		  0x00000020

#define CDMA_REG_OFFSET	 0x00010000
#define INTC_OFFSET		 0x000A0000
#define PCIE_CTRL_OFFSET	0x00000000

//#define VERBOSE
#ifdef VERBOSE
#define DPRINT(a...) pr_info("ni_cts3_daq: "); pr_cont(a); pr_cont(" (%s:%d)\n", __func__, __LINE__);
#else
#define DPRINT(a...)
#endif

struct PCIE_device
{
	int irq;

	void *mem_base_addr;
	unsigned long ddr_size;
	unsigned long mem_size;
	void *reg0_base_addr;
	void *reg1_base_addr;
	struct pci_dev *pci;

	wait_queue_head_t wait;	 /**< Device work queue */
	struct list_head list;	  /**< Node for the device list */

	struct file_operations fops;	/**< File operation structure for this device*/
	struct cdev cdev;		   /**< Character device */

	struct PCIE_dma Dma;
	u32 IrqPending;
	u32 IrqEnabled;
};

struct PCIE_file_data
{
	struct PCIE_device *pcie_dev;
	struct PCIE_cma_device cma_dev;
};

#define PCI_DEV_NUM 4
static LIST_HEAD(gPCIEDeviceList);
static DEFINE_SPINLOCK(gPCIEDeviceListLock);

static DECLARE_BITMAP(gMinors, PCI_DEV_NUM);

inline static u32 PCIE_INTCtrlReadReg(const struct PCIE_device *dev, u32 reg)
{
	u32 val = readl((void *) (dev->reg0_base_addr + INTC_OFFSET + reg));
	DPRINT("INTC R 0x%08x : 0x%08x", INTC_OFFSET + reg, val);
	return val;
}

inline static void PCIE_INTCtrlWriteReg(const struct PCIE_device *dev, u32 reg, u32 val)
{
	DPRINT("INTC W 0x%08x : 0x%08x", INTC_OFFSET + reg, val);
	writel(val, (void *) (dev->reg0_base_addr + INTC_OFFSET + reg));
}

inline static u32 PCIE_PCIeCtrlReadReg(const struct PCIE_device *dev, u32 reg)
{
	u32 val = readl((void *) (dev->reg1_base_addr + PCIE_CTRL_OFFSET + reg));
	DPRINT("PCIE R 0x%08x : 0x%08x", PCIE_CTRL_OFFSET + reg, val);
	return val;
}

inline static void PCIE_PCIeCtrlWriteReg(const struct PCIE_device *dev, u32 reg, u32 val)
{
	DPRINT("PCIE W 0x%08x : 0x%08x", PCIE_CTRL_OFFSET + reg, val);
	writel(val, (void *) (dev->reg1_base_addr + PCIE_CTRL_OFFSET + reg));
}

irqreturn_t PCIE_IRQHandler(int irq, void *data)
{
	struct PCIE_device * pDev = data;
	unsigned long flags;
	u32 Ipr;

	spin_lock_irqsave(&gPCIEDeviceListLock, flags);
	Ipr = PCIE_INTCtrlReadReg(pDev, INTC_REG_ISR);
	pDev->IrqPending |= (Ipr & ~INTC_CDMA_MASK);

	PCIE_INTCtrlWriteReg(pDev, INTC_REG_IAR, pDev->IrqPending); /* Acknowledge*/
	PCIE_INTCtrlWriteReg(pDev, INTC_REG_CIE, pDev->IrqPending);

	DPRINT("IRQ Occured : 0x%08X", Ipr);

	if((Ipr & INTC_CDMA_MASK) == INTC_CDMA_MASK)
	{
		PCI_CDMAIRQHandler(irq, &pDev->Dma);
		PCIE_INTCtrlWriteReg(pDev, INTC_REG_IAR, INTC_CDMA_MASK);
	}

//	pDev->IrqPending &= ~INTC_CDMA_MASK;
	spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);

	if(pDev->IrqPending)
		wake_up(&pDev->wait);

	return IRQ_HANDLED;
}

static int PCIE_MapResource(struct pci_dev *pci, struct PCIE_device *dev)
{
	void __iomem *resource_map[PCIE_NB_BAR];
	int ret, i;
	unsigned long start, len;

	memset(resource_map, 0, sizeof(resource_map));

	/*
	 * BAR 0 DDR
	 * BAR 1 Leds, buffer interrupt ctrl
	 * BAR 2 CDMA and pcie config registers
	 */
	/* MAP all BARs */
	for (i = 0; i < PCIE_NB_BAR; i++)
	{
/*		  if (i == PCIE_TEST_BAR)
			  continue; *//* BAR 1 not used */

		start = pci_resource_start(pci, i);
		len = pci_resource_len(pci, i);

		if (!request_mem_region(start, len, PCIE_PCI_DRIVER_NAME))
		{
			dev_err(&pci->dev,
					"Request mem region for BAR %d failed\n",
					i);
			ret = -EBUSY;
			goto error;
		}

		resource_map[i] = ioremap_nocache(start, len);
		if (resource_map[i] == NULL )
		{
			release_mem_region(start, len);
			dev_err(&pci->dev, "Map memory for BAR %d failed\n", i);
			ret = -EFAULT;
			goto error;
		}
	}

	dev->mem_base_addr = resource_map[PCIE_DDR_BAR];
	dev->mem_size = pci_resource_len(pci, PCIE_DDR_BAR);
	dev->reg0_base_addr = resource_map[PCIE_REG0_BAR];
	dev->reg1_base_addr = resource_map[PCIE_REG1_BAR];

	dev_info(&pci->dev, "Map memory mem base addr %p, size: %lu\n",
			dev->mem_base_addr, dev->mem_size);
	dev_info(&pci->dev, "Map memory reg0 base addr %p\n",
			dev->reg0_base_addr);
	dev_info(&pci->dev, "Map memory reg1 base addr %p\n",
			dev->reg1_base_addr);

	return 0;

error:
	for (i = 0; i < PCIE_NB_BAR && resource_map[i] != 0; i++)
	{
		iounmap(resource_map[i]);
		release_mem_region(pci_resource_start(pci, i),
				pci_resource_len(pci, i));
	}
	return ret;
}

static void PCIE_UnmapResource(struct pci_dev *pci, struct PCIE_device *dev)
{
	int i = 0;

	iounmap(dev->mem_base_addr);
	iounmap(dev->reg1_base_addr);

	for (i = 0; i < PCIE_NB_BAR; i++)
	{
		release_mem_region(pci_resource_start(pci, i),
				pci_resource_len(pci, i));
	}
}

static int PCIE_SetDMAMask(struct pci_dev *pci)
{
	int ret;

	if (!pci_set_dma_mask(pci, DMA_BIT_MASK(64)))
	{
		ret = pci_set_consistent_dma_mask(pci, DMA_BIT_MASK(64));
		if (ret)
		{
			ret = pci_set_consistent_dma_mask(pci,
					DMA_BIT_MASK(32));
			if (ret)
			{
				dev_err(&pci->dev, "Set 64 bits DMA failed\n");
				return ret;
			}
		}
	}
	else
	{
		ret = pci_set_dma_mask(pci, DMA_BIT_MASK(32));
		if (ret)
		{
			dev_err(&pci->dev, "Set 32 bits DMA failed\n");
			return ret;
		}
		ret = pci_set_consistent_dma_mask(pci, DMA_BIT_MASK(32));
		if (ret)
		{
			dev_err(&pci->dev,
					"Set consistent 32 bits DMA failed\n");
			return ret;
		}
	}

	return ret;
}

static void PCIE_SetDMAAddressTranslation(struct PCIE_device *pDev, struct PCIE_cma_device *cma)
{
	PCIE_PCIeCtrlWriteReg(pDev, AXIBAR2PCIEBAR_0U,
			0x00000000);
	PCIE_PCIeCtrlWriteReg(pDev, AXIBAR2PCIEBAR_0L,
			cma->phys_addr);
}


int PCIE_RegisterDevice(struct PCIE_device * pDev)
{
	int minor = 0;
	int ret = -ENODEV;
	int devno;
	unsigned long flags;

	minor = find_first_zero_bit(gMinors, PCI_DEV_NUM);
	if (pDev && PCI_DEV_NUM > minor)
	{
		/* Initialize and add char device */
		devno = MKDEV(PCIE_DRIVER_MAJOR, minor);
		cdev_init(&pDev->cdev, &pDev->fops);
		pDev->cdev.owner = THIS_MODULE;

		/* add device to list */
		spin_lock_irqsave(&gPCIEDeviceListLock, flags);
		list_add(&pDev->list, &gPCIEDeviceList);

		dev_info(&pDev->pci->dev, "Create device %d:%d\n",
				minor, PCIE_DRIVER_MAJOR);
		ret = cdev_add(&pDev->cdev, devno, 1);
		if (!ret)
		{
			/* Set bit for this minor number */
			set_bit(minor, gMinors);
			ret = 0;
		} else {
			list_del(&pDev->list);
		}
		spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
	}
	return ret;
}

void PCIE_UnregisterDevice( struct PCIE_device * pDdev)
{
	unsigned long flags;

	clear_bit(MINOR(pDdev->cdev.dev), gMinors);
	cdev_del(&pDdev->cdev);

	spin_lock_irqsave(&gPCIEDeviceListLock, flags);
	list_del(&pDdev->list);
	spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
}


inline static int PCIE_EnableIrq(const struct PCIE_device * pDev,
		uint32_t IrqMask)
{
	PCIE_INTCtrlWriteReg(pDev, INTC_REG_SIE, IrqMask);
	return 0;
}

inline static int PCIE_DisableIrq(const struct PCIE_device * pDev,
		uint32_t IrqMask)
{
	PCIE_INTCtrlWriteReg(pDev, INTC_REG_CIE, IrqMask);
	return 0;
}


static void PCIE_INTCtrlInit(const struct PCIE_device * pDev)
{
	PCIE_INTCtrlWriteReg(pDev, INTC_REG_MER, 0);
	PCIE_INTCtrlWriteReg(pDev, INTC_REG_IAR, 0xFFFFFFFF);

	/* Just enable CDMA irq */
	PCIE_INTCtrlWriteReg(pDev, INTC_REG_SIE, INTC_CDMA_MASK);

	/* Start interrupt ctrl */
	PCIE_INTCtrlWriteReg(pDev,
			INTC_REG_MER,
			INTC_MASTER_ENABLE_MASK |
			INTC_HARDWARE_ENABLE_MASK);

}

static struct PCIE_device *PCIE_GetDeviceFromDev_t(dev_t dev_t)
{
	struct PCIE_device *pDev = NULL;
	unsigned long Flags;

	spin_lock_irqsave(&gPCIEDeviceListLock, Flags);
	list_for_each_entry(pDev, &gPCIEDeviceList, list)
	{
		if (pDev->cdev.dev == dev_t)
		{
			break;
		}
	}
	spin_unlock_irqrestore(&gPCIEDeviceListLock, Flags);
	return pDev;
}

static int PCIE_Open(struct inode *inode, struct file *filp)
{
	struct PCIE_file_data *filedata;

	// Allocate file persistent data struct
	filedata = kzalloc(sizeof(filedata), GFP_KERNEL);
	if (!filedata)
		return -ENOMEM;

	filedata->pcie_dev = PCIE_GetDeviceFromDev_t(inode->i_rdev);
	if (!filedata->pcie_dev)
		return -ENXIO;

	filp->private_data = filedata;

	return 0;
}

static int PCIE_Release(struct inode *inode, struct file *filp)
{
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	unsigned long flags;

	spin_lock_irqsave(&gPCIEDeviceListLock, flags);

	// CMA buffer allocated ?
	if (filedata->cma_dev.phys_addr)
		PCI_FreeDMA(&filedata->pcie_dev->pci->dev, &filedata->cma_dev);

	// Free file persistent data struct
	kfree(filedata);

	filp->private_data = NULL;

	spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
	return 0;
}

static ssize_t PCIE_Read(struct file *filp, char *user_buf, size_t count, loff_t * f_pos)
{
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	struct PCIE_device *pDev = filedata->pcie_dev;
	ssize_t ret = 0;

	/* Check address in DDR */
	if((*f_pos + count) > pDev->ddr_size)
		count = pDev->ddr_size - *f_pos;

	if(count > 0) /* Always use DMA */
	{
		// Set Translation address
		PCIE_SetDMAAddressTranslation(pDev, &filedata->cma_dev);

		// Launch DMA
		ret = PCI_CDMARead(&pDev->Dma, &filedata->cma_dev, count, f_pos);
		if (ret > 0)
		{
			// Update offset
			*f_pos += ret;
		}
	}

	// End of offset
	if(*f_pos >= pDev->ddr_size)
	{
		*f_pos = 0;
	}

	return ret;
}



static ssize_t PCIE_Write(struct file *filp, const char *buf, size_t count, loff_t * f_pos)
{
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	struct PCIE_device *pDev = filedata->pcie_dev;
	ssize_t ret = 0;

	/* Check address in DDR */
	if((*f_pos + count) > pDev->ddr_size)
		count = pDev->ddr_size - *f_pos;

	if(count > 0) /* Always use DMA */
	{
		// Set Translation address
		PCIE_SetDMAAddressTranslation(pDev, &filedata->cma_dev);

		// Launch DMA
		ret = PCI_CDMAWrite(&pDev->Dma, &filedata->cma_dev, count, f_pos);
		if (ret > 0)
		{
			// Update offset
			*f_pos += ret;
		}
	}

	// End of offset
	if(*f_pos >= pDev->ddr_size)
	{
		*f_pos = 0;
	}

	return ret;
}

loff_t PCIE_Llseek(struct file *filp, loff_t off, int whence)
{
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	struct PCIE_device *pDev = filedata->pcie_dev;
	loff_t NewPos;

	switch (whence)
	{
	case 0: /* SEEK_SET */
		if(off <= pDev->ddr_size)
			NewPos = off;
		else
			NewPos = pDev->ddr_size;
		break;
	case 1: /* SEEK_CUR */
		if((filp->f_pos + off) <= pDev->ddr_size)
			NewPos = filp->f_pos + off;
		else
			NewPos = pDev->ddr_size;
		break;
	case 2: /* SEEK_END */
		NewPos = pDev->ddr_size;
		break;
	default: /* can't happen */
		return -EINVAL;
	}
	if (NewPos < 0)
		return -EINVAL;
	filp->f_pos = NewPos;

	return NewPos;
}


static unsigned int PCIE_Poll(struct file *filp, poll_table * wait)
{
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	struct PCIE_device *pDev = filedata->pcie_dev;
	unsigned int mask = 0;
	unsigned long flags;

	spin_lock_irqsave(&gPCIEDeviceListLock, flags);
	if(pDev->IrqPending /*& pDev->IrqEnabled*/)
	{
		mask |= POLLIN | POLLPRI | POLLRDNORM;	/* readable */
	}
	spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);

	if(!mask)
	{
		poll_wait(filp, &pDev->wait, wait);
		spin_lock_irqsave(&gPCIEDeviceListLock, flags);

		/* Read IRQ enable register and test active Irq */
		if(pDev->IrqPending /*& pDev->IrqEnabled*/)
		{
			mask |= POLLIN | POLLPRI | POLLRDNORM;	/* readable */
		}

		spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
	}

	return mask;
}

long PCIE_Ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, ret = 0;
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	struct PCIE_device *pDev = filedata->pcie_dev;
	unsigned long flags;

	/* Check command */
	if (_IOC_TYPE(cmd) != PCIE_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_NR(cmd) > PCIE_IOC_MAXNR)
		return -ENOTTY;

	/* Check access rights */
	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				_IOC_SIZE(cmd));
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				_IOC_SIZE(cmd));
	}

	if (err)
	{
		return -EFAULT;
	}

	/* Process ioctl */
	switch (cmd)
	{
	case PCIEIOC_GET_IRQ_STATUS:
	{
		spin_lock_irqsave(&gPCIEDeviceListLock, flags);
		__put_user(pDev->IrqPending, (uint32_t __user *)arg);
		spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
		break;
	}
	case PCIEIOC_GET_IRQ_MASK:
	{
		spin_lock_irqsave(&gPCIEDeviceListLock, flags);
		__put_user(pDev->IrqEnabled, (uint32_t __user *)arg);
		spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
		break;
	}
	case PCIEIOC_ENABLE_IRQ:
	{
		uint32_t IrqMask;

		ret = __get_user(IrqMask, (uint32_t __user *)arg);
		if(ret == 0)
		{
			spin_lock_irqsave(&gPCIEDeviceListLock, flags);
			pDev->IrqEnabled |= IrqMask;
			spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
			PCIE_EnableIrq(pDev, IrqMask);
		}

		break;
	}
	case PCIEIOC_DISABLE_IRQ:
	{
		uint32_t IrqMask;
		ret = __get_user(IrqMask, (uint32_t __user *)arg);
		if(ret == 0)
		{
			spin_lock_irqsave(&gPCIEDeviceListLock, flags);
			pDev->IrqEnabled &= ~IrqMask;
			spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
			PCIE_DisableIrq(pDev, IrqMask);
			PCIE_INTCtrlWriteReg(pDev, INTC_REG_IAR, IrqMask); /* Acknowledge*/
		}
		else
		{
			DPRINT("Get user data failed");
		}
		break;
	}
	case PCIEIOC_ACKNOWLEDGE_IRQ:
	{
		uint32_t IrqMask, Ipr;
		ret = __get_user(IrqMask, (uint32_t __user *)arg);

		if(ret == 0)
		{
			spin_lock_irqsave(&gPCIEDeviceListLock, flags);
			pDev->IrqPending &= ~IrqMask;
			Ipr = PCIE_INTCtrlReadReg(pDev, INTC_REG_ISR);
			pDev->IrqPending |= (Ipr & ~INTC_CDMA_MASK);
			PCIE_INTCtrlWriteReg(pDev, INTC_REG_IAR, pDev->IrqPending); /* Acknowledge*/
			if(pDev->IrqEnabled & IrqMask)
				PCIE_EnableIrq(pDev, IrqMask);
			spin_unlock_irqrestore(&gPCIEDeviceListLock, flags);
		}
		break;
	}
	default: /* Unknown IOCTL */
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static int PCIE_Mmap(struct file *filp, struct vm_area_struct *vma)
{
	int err;
	struct PCIE_file_data *filedata = (struct PCIE_file_data *) filp->private_data;
	size_t size;

	// Alloc CMA buffer
	size = vma->vm_end - vma->vm_start;

	// CMA buffer already allocated ?
	if (filedata->cma_dev.phys_addr)
		PCI_FreeDMA(&filedata->pcie_dev->pci->dev, &filedata->cma_dev);

	err = PCI_AllocDMA(&filedata->pcie_dev->pci->dev, &filedata->cma_dev, size);
	if (err)
	{
		pr_warn("ni_cts3_daq: Cannot alloc CMA buffer\n");
		return -ENOMEM;
	}

	// Remap allocated CMA buffer into user space
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	err = remap_pfn_range(vma, vma->vm_start, filedata->cma_dev.phys_addr >> PAGE_SHIFT, size, vma->vm_page_prot);
	if (err)
	{
		pr_err("ni_cts3_daq: Cannot remap buffer to mmap\n");
		return err;
	}

	return 0;
}

struct file_operations PCIE_OpsTemplate = {
	read:   PCIE_Read,
	write:  PCIE_Write,
	poll:   PCIE_Poll,
	unlocked_ioctl:PCIE_Ioctl,
	llseek: PCIE_Llseek,
	open:   PCIE_Open,
	release:PCIE_Release,
	mmap:   PCIE_Mmap,
};

static int PCIE_Probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	int ret = 0;
	struct PCIE_device *dev;

	/* Create new device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
	{
		dev_err(&pci->dev, "Memory allocation for device failed\n");
		return -ENOMEM;
	}

	/* Enable the device */
	if (pci_enable_device(pci))
	{
		dev_err(&pci->dev, "Enable device failed.\n");
		ret = -ENODEV;
		goto error_free;
	}
	dev_dbg(&pci->dev, "Device found\n");

	memcpy(&dev->fops, &PCIE_OpsTemplate, sizeof(dev->fops));

	dev->irq = pci->irq;
	dev->pci = pci;
	dev->ddr_size = DDR_SIZE;
	init_waitqueue_head(&dev->wait);

	/* Map PCI resources */
	ret = PCIE_MapResource(pci, dev);
	if (ret)
	{
		dev_err(&pci->dev, "Map pci resources failed\n");
		goto error_disable;
	}

	/* Configure DMA */
	ret = PCIE_SetDMAMask(pci);
	if (ret)
	{
		dev_err(&pci->dev, "Set DMA mask failed\n");
		goto error_unmap;
	}

	ret = PCI_CDMAInit(&pci->dev, &dev->Dma,
			(uint32_t)dev->reg1_base_addr + CDMA_REG_OFFSET);
	if (ret)
	{
		dev_err(&pci->dev, "DMA initialization failed\n");
		goto error_unmap;
	}

	/*Initialize interrupt controller */
	PCIE_INTCtrlInit(dev);

	//  Enable bus master
	pci_set_master(pci);

	/* We use legacy irq */
	pci_disable_msi(pci);

	/* Register irq */
	irq_set_irq_type(dev->irq, IRQ_TYPE_LEVEL_HIGH);

	ret = request_irq(dev->irq,
			&PCIE_IRQHandler,
			IRQF_SHARED,
			PCIE_PCI_DRIVER_NAME,
			dev);

	if (ret < 0)
	{
		dev_err(&pci->dev, "%s: Init: Unable to allocate IRQ\n",
				PCIE_PCI_DRIVER_NAME);
		goto error_unmap;
	}

	pci_set_drvdata(pci, dev);
	PCIE_RegisterDevice(dev);

	return 0;
//error_free_irq:
//	free_irq(dev->irq, pci);
error_unmap:
	PCIE_UnmapResource(pci, dev);
error_disable:
	pci_disable_device(pci);
error_free:
	kfree(dev);
	return ret;
}

static void PCIE_Remove(struct pci_dev *pci)
{
	struct PCIE_device *dev;

	dev = pci_get_drvdata(pci);
	pci_set_drvdata(pci, NULL );

	PCIE_UnregisterDevice(dev);

	free_irq(dev->irq, dev);

	if (pci_msi_enabled())
		pci_disable_msi(pci);

	PCIE_UnmapResource(pci, dev);
	pci_disable_device(pci);

	PCI_CDMAExit(&dev->Dma);

	kfree(dev);
	dev_dbg(&pci->dev, "Device removed\n");
}

/*
 * Device id table used for device detection
 */
static struct pci_device_id ids[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_XILINX),},
	{0,}
};

MODULE_DEVICE_TABLE(pci, ids);

static struct pci_driver gPCIEDriver = {
	.name = PCIE_PCI_DRIVER_NAME,
	.id_table = ids,
	.probe = PCIE_Probe,
	.remove = PCIE_Remove,
};

static int PCIE_Init(void)
{
	int ret = 0;

	ret = pci_register_driver(&gPCIEDriver);
	return ret;
}

static void PCIE_Exit(void)
{
	pci_unregister_driver(&gPCIEDriver);
}

module_init(PCIE_Init);
module_exit(PCIE_Exit);



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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

#define NI_SPYCDMA_DRIVER_MAJOR		242
#define NI_SPYCDMA_DRIVER_NAME		"ni_cts3_spy"

#define CDMA_REG_CDMACR				0x00	/* CDMA control register offset */
#define CDMA_REG_CDMASR				0x04	/* CDMA Status Register */
#define CDMA_REG_CURDESC_PNTR		0x08	/* Current Descriptor Pointer Register */
#define CDMA_REG_TAILDESC_PNTR		0x10	/* Tail Descriptor Pointer Register */
#define CDMA_REG_SA					0x18	/* Source Address Register */
#define CDMA_REG_DA					0x20	/* Destination Address Register */
#define CDMA_REG_BTT				0x28	/* bytes to Transfer Register */

#define CDMA_MASK_CDMASR_ERR_IRQ	(1 << 14)
#define CDMA_MASK_CDMASR_IOC_IRQ	(1 << 12)
#define CDMA_MASK_CDMASR_DMADECERR	(1 << 6)
#define CDMA_MASK_CDMASR_DMASLVERR	(1 << 4)
#define CDMA_MASK_CDMASR_DMAINTERR	(1 << 4)
#define CDMA_MASK_CDMASR_IDLE		(1 << 1)

#define CDMA_MASK_CDMACR_ERR_IRQEN	(1 << 14)
#define CDMA_MASK_CDMACR_IOC_IRQEN	(1 << 12)
#define CDMA_MASK_CDMACR_RESET		(1 << 2)

#define CDMA_DDRAXIADDR				0xC0000000
#define CDMA_DDR_SIZE				0x40000000 // 1 GiB

/**
 * @enum CDMA_STATUS
 * Status of the dma transfer
 */
enum CDMA_STATUS
{
	CDMA_STATUS_SUCCEED,	/**< DMA transfer succeed */
	CDMA_STATUS_ERROR,		/**< DMA transfer generic error */
	CDMA_STATUS_FREE,		/**< DMA free */
	CDMA_STATUS_BUSY,		/**< DMA busy */
};

struct spycdma_device
{
	struct device *dev;
	void __iomem *regs;
	int irq;
	struct cdev cdev;

	struct spinlock lock;		/* Used for avoiding concurrent access to the dma data */
	wait_queue_head_t wait;		/* Device work queue */
	enum CDMA_STATUS status;	/* DMA status */
};

struct spycdma_file_data
{
	struct spycdma_device *spycdma_dev;

	// CMA buffer
	size_t cma_size;			/* DMA buffer size */
	dma_addr_t cma_phys_addr;	/* DMA buffer physical start address*/
	void *cma_virt_addr;		/* DMA buffer virtual start address*/
};


inline static u32 spycdma_reg_read(struct spycdma_device *xdev, u32 reg)
{
	u32 val = ioread32(xdev->regs + reg);
	//pr_debug("ni_cts3_spy: reg r 0x%08x = 0x%08x\n", reg, val);
	return val;
}

inline static void spycdma_reg_write(struct spycdma_device *xdev, u32 reg, u32 val)
{
	//pr_debug("ni_cts3_spy: reg w 0x%08X = 0x%08X\n", reg, val);
	iowrite32(val, xdev->regs + reg);
}

static int spycdma_event(struct spycdma_device *xdev)
{
	u32 src;
	u32 dest;
	u32 val;
	u32 count;
	int ret = 0;

	pr_debug("ni_cts3_spy: Enter %s\n", __func__);

	/* 7. Determine the interrupt source */
	val = spycdma_reg_read(xdev, CDMA_REG_CDMASR);

	if ((val & CDMA_MASK_CDMASR_IOC_IRQ) == CDMA_MASK_CDMASR_IOC_IRQ)
	{
		/* 8. Clear the CDMASR.IOC_Irq bit by writing 1 to
		*	DMASR.IOC_Irq bit position*/
		spycdma_reg_write(xdev, CDMA_REG_CDMASR, val |
							CDMA_MASK_CDMASR_IOC_IRQ |
							CDMA_MASK_CDMASR_DMAINTERR |
							CDMA_MASK_CDMASR_DMADECERR);

		/* Read BTT acknowledge the interrupt */
		count = spycdma_reg_read(xdev, CDMA_REG_BTT);

		/* 9. Ready for another transfer */
		if ((val & CDMA_MASK_CDMASR_ERR_IRQ) ||
			(val & CDMA_MASK_CDMASR_DMAINTERR) ||
			(val & CDMA_MASK_CDMASR_DMASLVERR) ||
			(val & CDMA_MASK_CDMASR_DMADECERR))
		{
			/* Transfer failed */
			pr_err("ni_cts3_spy: DMA irq : Transfer failed CDMASR 0x%08X "
							"CDMASA 0x%08X CDMADA 0x%08X CDMABTT 0x%08X\n",
							val, spycdma_reg_read(xdev, CDMA_REG_SA),
							spycdma_reg_read(xdev, CDMA_REG_DA),
							spycdma_reg_read(xdev, CDMA_REG_BTT));

			xdev->status = CDMA_STATUS_ERROR;

			/* reset the CDMA */
			spycdma_reg_write(xdev, CDMA_REG_CDMACR, CDMA_MASK_CDMACR_RESET);
		}
		else
		{
			pr_debug("ni_cts3_spy: DMA irq : Transfer succeed\n");
			xdev->status = CDMA_STATUS_SUCCEED;

			src = spycdma_reg_read(xdev, CDMA_REG_SA);
			dest = spycdma_reg_read(xdev, CDMA_REG_DA);

			pr_debug("ni_cts3_spy: CDMA_REG_SA val 0x%08X\n", src);
			pr_debug("ni_cts3_spy: CDMA_REG_DA val 0x%08X\n", dest);
			pr_debug("ni_cts3_spy: DMA irq: DMA done\n");
		}
		/* Notify that DMA transfer is done */
		wake_up(&xdev->wait);
	}
	else
	{
		pr_err("ni_cts3_spy: Not a valid DMA IRQ\n");
		ret = -EAGAIN;
	}

	pr_debug("ni_cts3_spy: Leave %s\n", __func__);
	return ret;
}

irqreturn_t spycdma_irq_handler(int irq, void *dev)
{
	u32 cdma_isr;
	int dma_done = 0;
	irqreturn_t ret = IRQ_HANDLED;
	unsigned long flags;
	struct spycdma_device *xdev = (struct spycdma_device *)dev;

	pr_debug("ni_cts3_spy: Enter %s\n", __func__);

	spin_lock_irqsave(&xdev->lock, flags);

	spycdma_reg_write(xdev, CDMA_REG_CDMACR, 0);

	cdma_isr = spycdma_reg_read(xdev, CDMA_REG_CDMASR);
	if ((cdma_isr & CDMA_MASK_CDMASR_IOC_IRQ) == CDMA_MASK_CDMASR_IOC_IRQ)
	{
		if (xdev->status == CDMA_STATUS_BUSY && !spycdma_event(xdev))
		{
			dma_done = 1;
		}
	}

	if (dma_done == 0)
	{
		u32 val;

		/* Get status register */
		val = spycdma_reg_read(xdev, CDMA_REG_CDMASR);
		/* 8. Clear the CDMASR.IOC_Irq bit
		by writing 1 to DMASR.IOC_Irq bit position*/
		spycdma_reg_write(xdev, CDMA_REG_CDMASR,
						val |
						CDMA_MASK_CDMASR_IOC_IRQ |
						CDMA_MASK_CDMASR_DMAINTERR |
						CDMA_MASK_CDMASR_DMADECERR);

		/* Read BTT acknowledge the interrupt */
		spycdma_reg_read(xdev, CDMA_REG_BTT);

		pr_err("ni_cts3_spy: %s Unattended CDMA interrupt received done %d\n",
					__func__, dma_done);
		ret = IRQ_NONE;
	}

	/* Enable Irq */
	spycdma_reg_write(xdev, CDMA_REG_CDMACR,
					CDMA_MASK_CDMACR_IOC_IRQEN |
					CDMA_MASK_CDMACR_ERR_IRQEN);

	spin_unlock_irqrestore(&xdev->lock, flags);

	pr_debug("ni_cts3_spy: Leave %s\n", __func__);
	return ret;
}

static int spycdma_transfer(struct spycdma_file_data *filedata, int write, size_t count, loff_t *f_pos)
{
	u32 val;
	u32 src, dest;
	struct spycdma_device *xdev = filedata->spycdma_dev;

	pr_debug("ni_cts3_spy: Enter %s\n", __func__);
	if (count == 0)
	{
		pr_debug("ni_cts3_spy: Count = 0\n");
		return -EINVAL;
	}

	/* 1. Verify CDMASR.IDLE = 1
	Check that there is no transfer running */
	val = spycdma_reg_read(xdev, CDMA_REG_CDMASR);
	if (!(val & (CDMA_MASK_CDMASR_IDLE | CDMA_MASK_CDMASR_IOC_IRQ)))
	{
		pr_debug("ni_cts3_spy: %s DMA transfer already running\n", __func__);
		return -EBUSY;
	}

	/* 2. Program the CDMACR.IOC_IrqEn and CDMACR.ERR_IrqEn => Enable Irq */
	spycdma_reg_write(xdev, CDMA_REG_CDMACR,
						CDMA_MASK_CDMACR_IOC_IRQEN |
						CDMA_MASK_CDMACR_ERR_IRQEN);

	if (write)
	{
		src = filedata->cma_phys_addr;
		dest = (u32) (*f_pos + CDMA_DDRAXIADDR);
	}
	else
	{
		src = (u32) (*f_pos + CDMA_DDRAXIADDR);
		dest = filedata->cma_phys_addr;
	}

	/* 3. Write the source address */
	spycdma_reg_write(xdev, CDMA_REG_SA, src);

	/* 4. Write the transfer destination address */
	spycdma_reg_write(xdev, CDMA_REG_DA, dest);

	pr_debug("ni_cts3_spy: SRC = 0x%08x, DEST = 0x%08x\n", src, dest);

	xdev->status = CDMA_STATUS_BUSY;

	if (write)
	{
		pr_debug("ni_cts3_spy: Start CDMA transfer UC => FPGA\n");
	}
	else
	{
		pr_debug("ni_cts3_spy: Start CDMA transfer FPGA => UC\n");
	}

	/* 5. Write the number of bytes and start the transfer */
	spycdma_reg_write(xdev, CDMA_REG_BTT, count);

	return 0;
}

ssize_t spycdma_start_read(struct spycdma_file_data *filedata, size_t count, loff_t * ppos)
{
	ssize_t Ret = 0;
	unsigned long flags;
	struct spycdma_device *xdev = filedata->spycdma_dev;

	pr_debug("ni_cts3_spy: Enter %s\n", __func__);

	if(count > filedata->cma_size)
		count = filedata->cma_size;

	/* Lock DMA */
	spin_lock_irqsave(&xdev->lock, flags);

	Ret = spycdma_transfer(filedata, 0 /* Read */, count, ppos);
	if (!Ret)
	{
		pr_debug("ni_cts3_spy: Wait CDMA irq\n");
		spin_unlock_irqrestore(&xdev->lock, flags);

		Ret = wait_event_timeout(xdev->wait,
						xdev->status != CDMA_STATUS_BUSY, HZ*2);
		spin_lock_irqsave(&xdev->lock, flags);
		if (!Ret)
		{
			pr_err("ni_cts3_spy: Transfer timeout\n");
			/* reset the CDMA */
			spycdma_reg_write(xdev, CDMA_REG_CDMACR, CDMA_MASK_CDMACR_RESET);
			xdev->status = CDMA_STATUS_FREE;
			Ret = -EBUSY;
		}
		else if (xdev->status == CDMA_STATUS_SUCCEED)
		{
			pr_debug("ni_cts3_spy: DMA transfer succeed count = %u\n", count);
			Ret = count;
			xdev->status = CDMA_STATUS_FREE;
		}
		else
		{
			pr_debug("ni_cts3_spy: DMA transfer failed\n");
			/* reset the CDMA */
			spycdma_reg_write(xdev, CDMA_REG_CDMACR, CDMA_MASK_CDMACR_RESET);
			xdev->status = CDMA_STATUS_FREE;
			Ret = -EINVAL;
		}
	}

	spin_unlock_irqrestore(&xdev->lock, flags);

	pr_debug("ni_cts3_spy: Leave %s\n", __func__);

	return Ret;
}

ssize_t spycdma_start_write(struct spycdma_file_data *filedata, size_t count, loff_t * f_pos)
{
	ssize_t ret = 0;
	unsigned long flags;
	struct spycdma_device *xdev = filedata->spycdma_dev;

	pr_debug("ni_cts3_spy: Enter %s\n", __func__);

	/* DMA buffer size is max count for one transfer */
	if(count > filedata->cma_size)
		count = filedata->cma_size;

	/* Lock DMA */
	spin_lock_irqsave(&xdev->lock, flags);

	ret = spycdma_transfer(filedata, 1 /* Write */, count, f_pos);
	if (!ret)
	{
		spin_unlock_irqrestore(&xdev->lock, flags);
		ret = wait_event_timeout(xdev->wait,
						xdev->status != CDMA_STATUS_BUSY, HZ*2);
		spin_lock_irqsave(&xdev->lock, flags);
		if (!ret)
		{
			pr_err("ni_cts3_spy: Write transfer timeout dma.status\n");
			xdev->status = CDMA_STATUS_FREE;
			ret = -EBUSY;
		}
		else
		{
			pr_debug("ni_cts3_spy: DMA transfer succeed count = %u\n", count);
			ret = count;
		}
	}

	spin_unlock_irqrestore(&xdev->lock, flags);

	pr_debug("ni_cts3_spy: Leave %s\n", __func__);

	return ret;
}

static int spycdma_alloc_cma(struct spycdma_file_data *filedata, ssize_t size)
{
	int err = 0;

	pr_debug("ni_cts3_spy: Allocate CMA buffer\n");

	if (dma_set_mask_and_coherent(filedata->spycdma_dev->dev, DMA_BIT_MASK(32)))
	{
		pr_warn("ni_cts3_spy: No suitable CMA available\n");
		return -ENOMEM;
	}

	/* Allocate buffer for DMA transfer */
	filedata->cma_virt_addr = dma_alloc_coherent(filedata->spycdma_dev->dev, size, &filedata->cma_phys_addr, GFP_KERNEL);
	if (!filedata->cma_virt_addr)
	{
		filedata->cma_size = 0;
		pr_err("ni_cts3_spy: CMA buffer allocation failed size 0x%08X\n", size);
		err = -ENOMEM;
	}
	else
	{
		filedata->cma_size = size;
	}

	pr_debug("ni_cts3_spy: CMA Buffer Allocation: %#x -> %#x (size = %#x)\n",
		(unsigned int)filedata->cma_virt_addr,
		(unsigned int)filedata->cma_phys_addr,
		filedata->cma_size);
	return err;
}

static void spycdma_free_cma(struct spycdma_file_data *filedata)
{
	pr_debug("ni_cts3_spy: Free DMA buffer\n");

	/* Free buffer */
	if (filedata->cma_phys_addr)
	{
		dma_free_coherent(filedata->spycdma_dev->dev,
			filedata->cma_size,
			filedata->cma_virt_addr,
			filedata->cma_phys_addr);
	}
}

static int spycdma_fo_open(struct inode *inode, struct file *file)
{
	struct spycdma_file_data *filedata;

	pr_debug("ni_cts3_spy: open\n");

	// Allocate file persistent data struct
	filedata = kzalloc(sizeof(filedata), GFP_KERNEL);
	if (!filedata)
		return -ENOMEM;

	filedata->spycdma_dev = container_of(inode->i_cdev, struct spycdma_device, cdev);

	file->private_data = filedata;

	return 0;
}

static int spycdma_fo_release(struct inode *inode, struct file *file)
{
	struct spycdma_file_data *filedata = (struct spycdma_file_data *) file->private_data;

	pr_debug("ni_cts3_spy: release\n");

	// CMA buffer allocated ?
	if (filedata->cma_phys_addr)
		spycdma_free_cma(filedata);

	// Free file persistent data struct
	kfree(filedata);

	return 0;
}

static ssize_t spycdma_fo_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	ssize_t ret;
	struct spycdma_file_data *filedata = (struct spycdma_file_data *) file->private_data;

	pr_debug("ni_cts3_spy: read (size = %u, offset %llu)\n",
			size, *offset);

	// Check offset and DDR size
	if ((*offset + size) > CDMA_DDR_SIZE)
	{
		// Limit size
		size = CDMA_DDR_SIZE - *offset;
	}

	if (size > 0)
	{
		ret = spycdma_start_read(filedata, size, offset);
		if (ret > 0)
		{
			// Update offset
			*offset += ret;
		}
	}

	// End of offset
	if (*offset >= CDMA_DDR_SIZE)
	{
		*offset = 0;
	}

	return ret;
}

static ssize_t spycdma_fo_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset)
{
	ssize_t ret;
	struct spycdma_file_data *filedata = (struct spycdma_file_data *) file->private_data;

	pr_debug("ni_cts3_spy: write (size = %u, offset %llu)\n",
			size, *offset);

	// Check offset and DDR size
	if ((*offset + size) > CDMA_DDR_SIZE)
	{
		// Limit size
		size = CDMA_DDR_SIZE - *offset;
	}

	if (size > 0)
	{
		ret = spycdma_start_write(filedata, size, offset);
		if (ret > 0)
		{
			// Update offset
			*offset += ret;
		}
	}

	// End of offset
	if (*offset >= CDMA_DDR_SIZE)
	{
		*offset = 0;
	}

	return ret;
}

static int spycdma_fo_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err;
	struct spycdma_file_data *filedata = (struct spycdma_file_data *) file->private_data;
	size_t size;

	pr_debug("ni_cts3_spy: mmap\n");

	// Alloc CMA buffer
	size = vma->vm_end - vma->vm_start;

	// CMA buffer already allocated ?
	if (filedata->cma_phys_addr)
		spycdma_free_cma(filedata);

	err = spycdma_alloc_cma(filedata, size);
	if (err)
	{
		pr_warn("ni_cts3_spy: Cannot alloc CMA buffer\n");
		return -ENOMEM;
	}

	// Remap allocated CMA buffer into user space
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	err = remap_pfn_range(vma, vma->vm_start, filedata->cma_phys_addr >> PAGE_SHIFT, size, vma->vm_page_prot);
	if (err)
	{
		pr_err("ni_cts3_spy: Cannot remap buffer to mmap\n");
		return err;
	}

	return 0;
}

loff_t spycdma_fo_lseek(struct file *file, loff_t offset, int whence)
{
	loff_t new_offset;

	switch (whence)
	{
	case 0: /* SEEK_SET */
		if(offset <= CDMA_DDR_SIZE)
			new_offset = offset;
		else
			new_offset = CDMA_DDR_SIZE;
		break;
	case 1: /* SEEK_CUR */
		if((file->f_pos + offset) <= CDMA_DDR_SIZE)
			new_offset = file->f_pos + offset;
		else
			new_offset = CDMA_DDR_SIZE;
		break;
	case 2: /* SEEK_END */
		new_offset = CDMA_DDR_SIZE;
		break;
	default: /* can't happen */
		return -EINVAL;
	}
	if (new_offset < 0)
		return -EINVAL;

	// Update offset
	file->f_pos = new_offset;

	return new_offset;
}

struct file_operations spycdma_fops = {
	.owner = THIS_MODULE,
	.open = spycdma_fo_open,
	.release = spycdma_fo_release,
	.read = spycdma_fo_read,
	.write = spycdma_fo_write,
	.mmap = spycdma_fo_mmap,
	.llseek = spycdma_fo_lseek,
};

static int spycdma_probe(struct platform_device *pdev)
{
	int err;
	struct spycdma_device *xdev = NULL;
	struct resource *io = NULL;

	/* Allocate and initialize the DMA engine structure */
	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if (!xdev)
		return -ENOMEM;

	/* Request and map I/O memory */
	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!io)
	{
		dev_err(&pdev->dev, "could not get IO memory\n");
		return -ENXIO;
	}

	pr_debug("ni_cts3_spy: Base address = %#x\n", io->start);

	xdev->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(xdev->regs))
		return PTR_ERR(xdev->regs);

	/* Request IRQ */
	xdev->irq = platform_get_irq(pdev, 0);
	if (xdev->irq < 0)
	{
		dev_err(&pdev->dev, "could not get IRQ\n");
		return xdev->irq;
	}

	err = request_irq(xdev->irq, spycdma_irq_handler, IRQF_SHARED,
			NI_SPYCDMA_DRIVER_NAME, xdev);
	if (err < 0)
	{
		dev_err(&pdev->dev, "unable to request IRQ %d\n", xdev->irq);
		return err;
	}

	pr_debug("ni_cts3_spy: IRQ = %d\n", xdev->irq);

	// Init device var
	xdev->dev = &pdev->dev;
	xdev->status = CDMA_STATUS_FREE;
	spin_lock_init(&xdev->lock);
	init_waitqueue_head(&xdev->wait);

	// Save driver data
	platform_set_drvdata(pdev, xdev);

	// Register character device
	err = register_chrdev_region(MKDEV(NI_SPYCDMA_DRIVER_MAJOR, 0),
							1, NI_SPYCDMA_DRIVER_NAME);
	if (err)
	{
		pr_err("ni_cts3_spy: Cannot register cdev\n");
		return err;
	}

	// Initialize cdev field
	cdev_init(&xdev->cdev, &spycdma_fops);
	cdev_add(&xdev->cdev, MKDEV(NI_SPYCDMA_DRIVER_MAJOR, 0), 1);

	return err;
}

static int spycdma_remove(struct platform_device *pdev)
{
	struct spycdma_device *xdev = platform_get_drvdata(pdev);

	// Release cdev fields
	cdev_del(&xdev->cdev);
	unregister_chrdev_region(MKDEV(NI_SPYCDMA_DRIVER_MAJOR, 0), 1);

	// Free IRQ
	if (xdev->irq > 0)
		free_irq(xdev->irq, xdev);

	return 0;
}

static const struct of_device_id spycdma_of_ids[] = {
	{ .compatible = "ni,spy-cdma", },
	{}
};

static struct platform_driver spycdma_driver = {
	.driver = {
		.name = "spycdma",
		.owner = THIS_MODULE,
		.of_match_table = spycdma_of_ids,
	},
	.probe = spycdma_probe,
	.remove = spycdma_remove,
};

static int __init spycdma_init(void)
{
	return platform_driver_register(&spycdma_driver);
}

static void __exit spycdma_exit(void)
{
	platform_driver_unregister(&spycdma_driver);
}

module_init(spycdma_init);
module_exit(spycdma_exit)

MODULE_AUTHOR("Guillaume Fouilleul <guillaume.fouilleul@ni.com>");
MODULE_DESCRIPTION("NI SPY CDMA device driver");
MODULE_LICENSE("GPL v2");
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

#ifndef PCIDRIVER_H_
#define PCIDRIVER_H_

#include <linux/ioctl.h>

/* Ioctls */
#define PCIE_IOC_MAGIC  240 /* Magic number*/

#define PCIEIOC_GET_IRQ_STATUS  _IOR(PCIE_IOC_MAGIC, 0, uint32_t)
#define PCIEIOC_GET_IRQ_MASK    _IOR(PCIE_IOC_MAGIC, 1, uint32_t)
#define PCIEIOC_ENABLE_IRQ      _IOW(PCIE_IOC_MAGIC, 2, uint32_t)
#define PCIEIOC_DISABLE_IRQ     _IOW(PCIE_IOC_MAGIC, 3, uint32_t)
#define PCIEIOC_ACKNOWLEDGE_IRQ _IOW(PCIE_IOC_MAGIC, 4, uint32_t)

#define PCIE_IOC_MAXNR 4

#endif /* PCIDRIVER_H_ */

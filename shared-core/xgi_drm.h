/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ***************************************************************************/

#ifndef _XGI_DRM_H_
#define _XGI_DRM_H_

#include <linux/types.h>
#include <asm/ioctl.h>

struct drm_xgi_sarea {
	__u16 device_id;
	__u16 vendor_id;

	char device_name[32];

	unsigned int scrn_start;
	unsigned int scrn_xres;
	unsigned int scrn_yres;
	unsigned int scrn_bpp;
	unsigned int scrn_pitch;
};


struct xgi_bootstrap {
	/**
	 * Size of PCI-e GART range in megabytes.
	 */
	unsigned int gart_size;
};


enum xgi_mem_location {
	XGI_MEMLOC_NON_LOCAL = 0,
	XGI_MEMLOC_LOCAL = 1,
	XGI_MEMLOC_INVALID = 0x7fffffff
};

struct xgi_mem_alloc {
	unsigned int location;
	unsigned int size;
	unsigned int is_front;
	unsigned int owner;

	/**
	 * Address of the memory from the graphics hardware's point of view.
	 */
	__u32 hw_addr;

	/**
	 * Offset of the allocation in the mapping.
	 */
	__u32 offset;
};

enum xgi_batch_type {
	BTYPE_2D = 0,
	BTYPE_3D = 1,
	BTYPE_FLIP = 2,
	BTYPE_CTRL = 3,
	BTYPE_NONE = 0x7fffffff
};

struct xgi_cmd_info {
	__u32 type;
	__u32 hw_addr;
	__u32 size;
	__u32 id;
};

struct xgi_state_info {
	unsigned int _fromState;
	unsigned int _toState;
};


/*
 * Ioctl definitions
 */

#define DRM_XGI_BOOTSTRAP           0
#define DRM_XGI_FB_ALLOC            1
#define DRM_XGI_FB_FREE             2
#define DRM_XGI_PCIE_ALLOC          3
#define DRM_XGI_PCIE_FREE           4
#define DRM_XGI_SUBMIT_CMDLIST      5
#define DRM_XGI_GE_RESET            6
#define DRM_XGI_DUMP_REGISTER       7
#define DRM_XGI_DEBUG_INFO          8
#define DRM_XGI_TEST_RWINKERNEL     9
#define DRM_XGI_STATE_CHANGE        10

#define XGI_IOCTL_BOOTSTRAP         DRM_IOW(DRM_COMMAND_BASE + DRM_XGI_BOOTSTRAP, struct xgi_bootstrap)

#define XGI_IOCTL_FB_ALLOC          DRM_IOWR(DRM_COMMAND_BASE + DRM_XGI_FB_ALLOC, struct xgi_mem_alloc)
#define XGI_IOCTL_FB_FREE           DRM_IOW(DRM_COMMAND_BASE + DRM_XGI_FB_FREE, __u32)

#define XGI_IOCTL_PCIE_ALLOC        DRM_IOWR(DRM_COMMAND_BASE + DRM_XGI_PCIE_ALLOC, struct xgi_mem_alloc)
#define XGI_IOCTL_PCIE_FREE         DRM_IOW(DRM_COMMAND_BASE + DRM_XGI_PCIE_FREE, __u32)

#define XGI_IOCTL_GE_RESET          DRM_IO(DRM_COMMAND_BASE + DRM_XGI_GE_RESET)
#define XGI_IOCTL_DUMP_REGISTER     DRM_IO(DRM_COMMAND_BASE + DRM_XGI_DUMP_REGISTER)
#define XGI_IOCTL_DEBUG_INFO        DRM_IO(DRM_COMMAND_BASE + DRM_XGI_DEBUG_INFO)
#define XGI_IOCTL_SUBMIT_CMDLIST    DRM_IOW(DRM_COMMAND_BASE + DRM_XGI_SUBMIT_CMDLIST, struct xgi_cmd_info)
#define XGI_IOCTL_TEST_RWINKERNEL   DRM_IOW(DRM_COMMAND_BASE + DRM_XGI_TEST_RWINKERNEL, __u32)
#define XGI_IOCTL_STATE_CHANGE      DRM_IOW(DRM_COMMAND_BASE + DRM_XGI_STATE_CHANGE, struct xgi_state_info)

#endif /* _XGI_DRM_H_ */

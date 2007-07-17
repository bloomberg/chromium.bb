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
	 * Physical address of the memory from the processor's point of view.
	 */
	unsigned long bus_addr;
};

struct xgi_sarea_info {
	unsigned long bus_addr;
	unsigned int size;
};

enum xgi_batch_type {
	BTYPE_2D = 0,
	BTYPE_3D = 1,
	BTYPE_FLIP = 2,
	BTYPE_CTRL = 3,
	BTYPE_NONE = 0x7fffffff
};

struct xgi_cmd_info {
	unsigned int  _firstBeginType;
	__u32 _firstBeginAddr;
	__u32 _firstSize;
	__u32 _curDebugID;
	__u32 _lastBeginAddr;
	unsigned int _beginCount;

};

struct xgi_state_info {
	unsigned int _fromState;
	unsigned int _toState;
};

struct xgi_mmio_info {
	unsigned long mmio_base;
	unsigned int size;
};


/*
 * Ioctl definitions
 */

#define XGI_IOCTL_MAGIC             'x'	/* use 'x' as magic number */

#define XGI_IOCTL_BASE              0
#define XGI_ESC_POST_VBIOS          (XGI_IOCTL_BASE + 0)

#define XGI_ESC_FB_ALLOC            (XGI_IOCTL_BASE + 1)
#define XGI_ESC_FB_FREE             (XGI_IOCTL_BASE + 2)
#define XGI_ESC_PCIE_ALLOC          (XGI_IOCTL_BASE + 3)
#define XGI_ESC_PCIE_FREE           (XGI_IOCTL_BASE + 4)
#define XGI_ESC_SUBMIT_CMDLIST      (XGI_IOCTL_BASE + 5)
#define XGI_ESC_GE_RESET            (XGI_IOCTL_BASE + 6)
#define XGI_ESC_DUMP_REGISTER       (XGI_IOCTL_BASE + 7)
#define XGI_ESC_DEBUG_INFO          (XGI_IOCTL_BASE + 8)
#define XGI_ESC_TEST_RWINKERNEL     (XGI_IOCTL_BASE + 9)
#define XGI_ESC_STATE_CHANGE        (XGI_IOCTL_BASE + 10)

#define XGI_IOCTL_POST_VBIOS        _IO(XGI_IOCTL_MAGIC, XGI_ESC_POST_VBIOS)

#define XGI_IOCTL_FB_ALLOC          _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_FB_ALLOC, struct xgi_mem_alloc)
#define XGI_IOCTL_FB_FREE           _IOW(XGI_IOCTL_MAGIC, XGI_ESC_FB_FREE, unsigned long)

#define XGI_IOCTL_PCIE_ALLOC        _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_ALLOC, struct xgi_mem_alloc)
#define XGI_IOCTL_PCIE_FREE         _IOW(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_FREE, unsigned long)

#define XGI_IOCTL_GE_RESET          _IO(XGI_IOCTL_MAGIC, XGI_ESC_GE_RESET)
#define XGI_IOCTL_DUMP_REGISTER     _IO(XGI_IOCTL_MAGIC, XGI_ESC_DUMP_REGISTER)
#define XGI_IOCTL_DEBUG_INFO        _IO(XGI_IOCTL_MAGIC, XGI_ESC_DEBUG_INFO)

#define XGI_IOCTL_SUBMIT_CMDLIST    _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_SUBMIT_CMDLIST, struct xgi_cmd_info)
#define XGI_IOCTL_TEST_RWINKERNEL   _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_TEST_RWINKERNEL, unsigned long)
#define XGI_IOCTL_STATE_CHANGE      _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_STATE_CHANGE, struct xgi_state_info)

#define XGI_IOCTL_MAXNR          30

/*
 * flags
 */
#define XGI_FLAG_OPEN            0x0001
#define XGI_FLAG_NEEDS_POSTING   0x0002
#define XGI_FLAG_WAS_POSTED      0x0004
#define XGI_FLAG_CONTROL         0x0010
#define XGI_FLAG_MAP_REGS_EARLY  0x0200


#endif /* _XGI_DRM_H_ */

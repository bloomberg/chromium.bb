/* mach64.h -- ATI Mach 64 DRM template customization -*- linux-c -*-
 * Created: Wed Feb 14 16:07:10 2001 by gareth@valinux.com
 *
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * Copyright 2002-2003 Leif Delgass
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Leif Delgass <ldelgass@retinalburn.net>
 */

#ifndef __MACH64_H__
#define __MACH64_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) mach64_##x

/* General customization:
 */

#define DRIVER_AUTHOR		"Gareth Hughes, Leif Delgass, José Fonseca"

#define DRIVER_NAME		"mach64"
#define DRIVER_DESC		"DRM module for the ATI Rage Pro"
#define DRIVER_DATE		"20020904"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

/* Interface history:
 *
 * 1.0 - Initial mach64 DRM
 *
 */
#define DRIVER_IOCTLS									\
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	          = { mach64_dma_buffers,    1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_INIT)]     = { mach64_dma_init,       1, 1 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_CLEAR)]    = { mach64_dma_clear,      1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_SWAP)]     = { mach64_dma_swap,       1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_IDLE)]     = { mach64_dma_idle,       1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_RESET)]    = { mach64_engine_reset,   1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_VERTEX)]   = { mach64_dma_vertex,     1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_BLIT)]     = { mach64_dma_blit,       1, 0 },	\
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_FLUSH)]    = { mach64_dma_flush,      1, 0 },    \
   	[DRM_IOCTL_NR(DRM_IOCTL_MACH64_GETPARAM)] = { mach64_get_param,      1, 0 }

#endif

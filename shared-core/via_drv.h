/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _VIA_DRV_H_
#define _VIA_DRV_H_

#include "via_drm.h"


typedef struct drm_via_private {
        drm_via_sarea_t *sarea_priv;
        drm_map_t *sarea;
        drm_map_t *fb;
        drm_map_t *mmio;
        unsigned long agpAddr;
        drm_map_t *buffers;
        wait_queue_head_t decoder_queue[VIA_NR_XVMC_LOCKS];
} drm_via_private_t;

 
/* VIA MMIO register access */
#define VIA_BASE ((dev_priv->mmio))

#define VIA_READ(reg)		DRM_READ32(VIA_BASE, reg)
#define VIA_WRITE(reg,val)	DRM_WRITE32(VIA_BASE, reg, val)
#define VIA_READ8(reg)		DRM_READ8(VIA_BASE, reg)
#define VIA_WRITE8(reg,val)	DRM_WRITE8(VIA_BASE, reg, val)

 
extern int via_do_init_map(drm_device_t *dev, drm_via_init_t *init);
extern int via_do_cleanup_map(drm_device_t *dev);
extern int via_map_init(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);

#endif

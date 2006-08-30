/*
 * Copyright 2005 Stephane Marchesin.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __NOUVEAU_DRM_H__
#define __NOUVEAU_DRM_H__

typedef struct drm_nouveau_fifo_init {
	int          channel;
	uint32_t     put_base;
	/* FIFO control regs */
	drm_handle_t ctrl;
	int          ctrl_size;
	/* DMA command buffer */
	drm_handle_t cmdbuf;
	int          cmdbuf_size;
}
drm_nouveau_fifo_init_t;

typedef struct drm_nouveau_object_init {
	uint32_t handle;
	int class;
	uint32_t flags0, flags1, flags2;
	/* these are object handles */
	uint32_t dma0;
	uint32_t dma1;
	uint32_t dma_notifier;
}
drm_nouveau_object_init_t;

typedef struct drm_nouveau_dma_object_init {
	uint32_t handle;
	int      access;
	int      target;
	uint32_t offset;
	int      size;
}
drm_nouveau_dma_object_init_t;

#define NOUVEAU_MEM_FB			0x00000001
#define NOUVEAU_MEM_AGP			0x00000002
#define NOUVEAU_MEM_FB_ACCEPTABLE	0x00000004
#define NOUVEAU_MEM_AGP_ACCEPTABLE	0x00000008
#define NOUVEAU_MEM_PINNED		0x00000010
#define NOUVEAU_MEM_USER_BACKED		0x00000020
#define NOUVEAU_MEM_MAPPED		0x00000040

typedef struct drm_nouveau_mem_alloc {
	int flags;
	int alignment;
	uint64_t size;	// in bytes
	uint64_t __user *region_offset;
}
drm_nouveau_mem_alloc_t;

typedef struct drm_nouveau_mem_free {
	int flags;
	uint64_t region_offset;
}
drm_nouveau_mem_free_t;

typedef struct drm_nouveau_getparam {
	unsigned int param;
	unsigned int value;
}
drm_nouveau_getparam_t;

typedef struct drm_nouveau_setparam {
	unsigned int param;
	unsigned int value;
}
drm_nouveau_setparam_t;

enum nouveau_card_type {
	NV_UNKNOWN =0,
	NV_01      =1,
	NV_03      =3,
	NV_04      =4,
	NV_05      =5,
	NV_10      =10,
	NV_20      =20,
	NV_30      =30,
	NV_40      =40,
	G_70       =50,
	NV_LAST    =0xffff,
};

enum nouveau_bus_type {
	NV_AGP     =0,
	NV_PCI     =1,
	NV_PCIE    =2,
};

#define NOUVEAU_MAX_SAREA_CLIPRECTS 16

typedef struct drm_nouveau_sarea {
	/* the cliprects */
	drm_clip_rect_t boxes[NOUVEAU_MAX_SAREA_CLIPRECTS];
	unsigned int nbox;
}
drm_nouveau_sarea_t;

#define DRM_NOUVEAU_FIFO_INIT       0x00
#define DRM_NOUVEAU_PFIFO_REINIT    0x01
#define DRM_NOUVEAU_OBJECT_INIT     0x02
#define DRM_NOUVEAU_DMA_OBJECT_INIT 0x03 // We don't want this eventually..
#define DRM_NOUVEAU_MEM_ALLOC       0x04
#define DRM_NOUVEAU_MEM_FREE        0x05
#define DRM_NOUVEAU_GETPARAM        0x06
#define DRM_NOUVEAU_SETPARAM        0x07

#endif /* __NOUVEAU_DRM_H__ */


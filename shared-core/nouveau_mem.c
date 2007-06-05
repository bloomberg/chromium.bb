/*
 * Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.
 * Copyright 2005 Stephane Marchesin
 *
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
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
 * THE AUTHORS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */


#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "nouveau_drv.h"

static struct mem_block *split_block(struct mem_block *p, uint64_t start, uint64_t size,
		DRMFILE filp)
{
	/* Maybe cut off the start of an existing block */
	if (start > p->start) {
		struct mem_block *newblock =
			drm_alloc(sizeof(*newblock), DRM_MEM_BUFS);
		if (!newblock)
			goto out;
		newblock->start = start;
		newblock->size = p->size - (start - p->start);
		newblock->filp = NULL;
		newblock->next = p->next;
		newblock->prev = p;
		p->next->prev = newblock;
		p->next = newblock;
		p->size -= newblock->size;
		p = newblock;
	}

	/* Maybe cut off the end of an existing block */
	if (size < p->size) {
		struct mem_block *newblock =
			drm_alloc(sizeof(*newblock), DRM_MEM_BUFS);
		if (!newblock)
			goto out;
		newblock->start = start + size;
		newblock->size = p->size - size;
		newblock->filp = NULL;
		newblock->next = p->next;
		newblock->prev = p;
		p->next->prev = newblock;
		p->next = newblock;
		p->size = size;
	}

out:
	/* Our block is in the middle */
	p->filp = filp;
	return p;
}

static struct mem_block *alloc_block(struct mem_block *heap, uint64_t size,
		int align2, DRMFILE filp)
{
	struct mem_block *p;
	uint64_t mask = (1 << align2) - 1;

	if (!heap)
		return NULL;

	list_for_each(p, heap) {
		uint64_t start = (p->start + mask) & ~mask;
		if (p->filp == 0 && start + size <= p->start + p->size)
			return split_block(p, start, size, filp);
	}

	return NULL;
}

static struct mem_block *find_block(struct mem_block *heap, uint64_t start)
{
	struct mem_block *p;

	list_for_each(p, heap)
		if (p->start == start)
			return p;

	return NULL;
}

static void free_block(struct mem_block *p)
{
	p->filp = NULL;

	/* Assumes a single contiguous range.  Needs a special filp in
	 * 'heap' to stop it being subsumed.
	 */
	if (p->next->filp == 0) {
		struct mem_block *q = p->next;
		p->size += q->size;
		p->next = q->next;
		p->next->prev = p;
		drm_free(q, sizeof(*q), DRM_MEM_BUFS);
	}

	if (p->prev->filp == 0) {
		struct mem_block *q = p->prev;
		q->size += p->size;
		q->next = p->next;
		q->next->prev = q;
		drm_free(p, sizeof(*q), DRM_MEM_BUFS);
	}
}

/* Initialize.  How to check for an uninitialized heap?
 */
static int init_heap(struct mem_block **heap, uint64_t start, uint64_t size)
{
	struct mem_block *blocks = drm_alloc(sizeof(*blocks), DRM_MEM_BUFS);

	if (!blocks)
		return DRM_ERR(ENOMEM);

	*heap = drm_alloc(sizeof(**heap), DRM_MEM_BUFS);
	if (!*heap) {
		drm_free(blocks, sizeof(*blocks), DRM_MEM_BUFS);
		return DRM_ERR(ENOMEM);
	}

	blocks->start = start;
	blocks->size = size;
	blocks->filp = NULL;
	blocks->next = blocks->prev = *heap;

	memset(*heap, 0, sizeof(**heap));
	(*heap)->filp = (DRMFILE) - 1;
	(*heap)->next = (*heap)->prev = blocks;
	return 0;
}

/* 
 * Free all blocks associated with the releasing filp
 */
void nouveau_mem_release(DRMFILE filp, struct mem_block *heap)
{
	struct mem_block *p;

	if (!heap || !heap->next)
		return;

	list_for_each(p, heap) {
		if (p->filp == filp)
			p->filp = NULL;
	}

	/* Assumes a single contiguous range.  Needs a special filp in
	 * 'heap' to stop it being subsumed.
	 */
	list_for_each(p, heap) {
		while ((p->filp == 0) && (p->next->filp == 0) && (p->next!=heap)) {
			struct mem_block *q = p->next;
			p->size += q->size;
			p->next = q->next;
			p->next->prev = p;
			drm_free(q, sizeof(*q), DRM_MEM_DRIVER);
		}
	}
}

/* 
 * Cleanup everything
 */
static void nouveau_mem_takedown(struct mem_block **heap)
{
	struct mem_block *p;

	if (!*heap)
		return;

	for (p = (*heap)->next; p != *heap;) {
		struct mem_block *q = p;
		p = p->next;
		drm_free(q, sizeof(*q), DRM_MEM_DRIVER);
	}

	drm_free(*heap, sizeof(**heap), DRM_MEM_DRIVER);
	*heap = NULL;
}

void nouveau_mem_close(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_mem_takedown(&dev_priv->agp_heap);
	nouveau_mem_takedown(&dev_priv->fb_heap);
}

/* returns the amount of FB ram in bytes */
uint64_t nouveau_mem_fb_amount(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	switch(dev_priv->card_type)
	{
		case NV_03:
			switch(NV_READ(NV03_BOOT_0)&NV03_BOOT_0_RAM_AMOUNT)
			{
				case NV03_BOOT_0_RAM_AMOUNT_8MB:
				case NV03_BOOT_0_RAM_AMOUNT_8MB_SDRAM:
					return 8*1024*1024;
				case NV03_BOOT_0_RAM_AMOUNT_4MB:
					return 4*1024*1024;
				case NV03_BOOT_0_RAM_AMOUNT_2MB:
					return 2*1024*1024;
			}
			break;
		case NV_04:
		case NV_05:
			if (NV_READ(NV03_BOOT_0) & 0x00000100) {
				return (((NV_READ(NV03_BOOT_0) >> 12) & 0xf)*2+2)*1024*1024;
			} else
			switch(NV_READ(NV03_BOOT_0)&NV03_BOOT_0_RAM_AMOUNT)
			{
				case NV04_BOOT_0_RAM_AMOUNT_32MB:
					return 32*1024*1024;
				case NV04_BOOT_0_RAM_AMOUNT_16MB:
					return 16*1024*1024;
				case NV04_BOOT_0_RAM_AMOUNT_8MB:
					return 8*1024*1024;
				case NV04_BOOT_0_RAM_AMOUNT_4MB:
					return 4*1024*1024;
			}
			break;
		case NV_10:
		case NV_17:
		case NV_20:
		case NV_30:
		case NV_40:
		case NV_44:
		case NV_50:
		default:
			// XXX won't work on BSD because of pci_read_config_dword
			if (dev_priv->flags&NV_NFORCE) {
				uint32_t mem;
				pci_read_config_dword(dev->pdev, 0x7C, &mem);
				return (uint64_t)(((mem >> 6) & 31) + 1)*1024*1024;
			} else if(dev_priv->flags&NV_NFORCE2) {
				uint32_t mem;
				pci_read_config_dword(dev->pdev, 0x84, &mem);
				return (uint64_t)(((mem >> 4) & 127) + 1)*1024*1024;
			} else {
				uint64_t mem;
				mem=(NV_READ(NV04_FIFO_DATA)&NV10_FIFO_DATA_RAM_AMOUNT_MB_MASK) >> NV10_FIFO_DATA_RAM_AMOUNT_MB_SHIFT;
				return mem*1024*1024;
			}
			break;
	}

	DRM_ERROR("Unable to detect video ram size. Please report your setup to " DRIVER_EMAIL "\n");
	return 0;
}



int nouveau_mem_init(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fb_size;
	dev_priv->agp_phys=0;
	dev_priv->fb_phys=0;

	/* init AGP */
	dev_priv->agp_heap=NULL;
	if (drm_device_is_agp(dev))
	{
		int err;
		drm_agp_info_t info;
		drm_agp_mode_t mode;
		drm_agp_buffer_t agp_req;
		drm_agp_binding_t bind_req;

		err = drm_agp_acquire(dev);
		if (err) {
			DRM_ERROR("Unable to acquire AGP: %d\n", err);
			goto no_agp;
		}

		err = drm_agp_info(dev, &info);
		if (err) {
			DRM_ERROR("Unable to get AGP info: %d\n", err);
			goto no_agp;
		}

		/* see agp.h for the AGPSTAT_* modes available */
		mode.mode = info.mode;
		err = drm_agp_enable(dev, mode);
		if (err) {
			DRM_ERROR("Unable to enable AGP: %d\n", err);
			goto no_agp;
		}

		agp_req.size = info.aperture_size;
		agp_req.type = 0;
		err = drm_agp_alloc(dev, &agp_req);
		if (err) {
			DRM_ERROR("Unable to alloc AGP: %d\n", err);
			goto no_agp;
		}

		bind_req.handle = agp_req.handle;
		bind_req.offset = 0;
		err = drm_agp_bind(dev, &bind_req);
		if (err) {
			DRM_ERROR("Unable to bind AGP: %d\n", err);
			goto no_agp;
		}

		if (init_heap(&dev_priv->agp_heap, info.aperture_base, info.aperture_size))
			goto no_agp;

		dev_priv->agp_phys		= info.aperture_base;
		dev_priv->agp_available_size	= info.aperture_size;
	}
no_agp:

	/* setup a mtrr over the FB */
	dev_priv->fb_mtrr = drm_mtrr_add(drm_get_resource_start(dev, 1),
					 nouveau_mem_fb_amount(dev),
					 DRM_MTRR_WC);

	/* Init FB */
	dev_priv->fb_phys=drm_get_resource_start(dev,1);
	fb_size = nouveau_mem_fb_amount(dev);
	/* On at least NV40, RAMIN is actually at the end of vram.
	 * We don't want to allocate this... */
	if (dev_priv->card_type >= NV_40)
		fb_size -= dev_priv->ramin_size;
	dev_priv->fb_available_size = fb_size;
	DRM_DEBUG("Available VRAM: %dKiB\n", fb_size>>10);

	if (fb_size>256*1024*1024) {
		/* On cards with > 256Mb, you can't map everything. 
		 * So we create a second FB heap for that type of memory */
		if (init_heap(&dev_priv->fb_heap, drm_get_resource_start(dev,1), 256*1024*1024))
			return DRM_ERR(ENOMEM);
		if (init_heap(&dev_priv->fb_nomap_heap, drm_get_resource_start(dev,1)+256*1024*1024, fb_size-256*1024*1024))
			return DRM_ERR(ENOMEM);
	} else {
		if (init_heap(&dev_priv->fb_heap, drm_get_resource_start(dev,1), fb_size))
			return DRM_ERR(ENOMEM);
		dev_priv->fb_nomap_heap=NULL;
	}

	return 0;
}

struct mem_block* nouveau_mem_alloc(struct drm_device *dev, int alignment, uint64_t size, int flags, DRMFILE filp)
{
	struct mem_block *block;
	int type;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/* 
	 * Make things easier on ourselves: all allocations are page-aligned. 
	 * We need that to map allocated regions into the user space
	 */
	if (alignment < PAGE_SHIFT)
		alignment = PAGE_SHIFT;

	/*
	 * Warn about 0 sized allocations, but let it go through. It'll return 1 page
	 */
	if (size == 0)
		DRM_INFO("warning : 0 byte allocation\n");

	/*
	 * Keep alloc size a multiple of the page size to keep drm_addmap() happy
	 */
	if (size & (~PAGE_MASK))
		size = ((size/PAGE_SIZE) + 1) * PAGE_SIZE;

	if (flags&NOUVEAU_MEM_AGP) {
		type=NOUVEAU_MEM_AGP;
		block = alloc_block(dev_priv->agp_heap, size, alignment, filp);
		if (block) goto alloc_ok;
	}
	if (flags&(NOUVEAU_MEM_FB|NOUVEAU_MEM_FB_ACCEPTABLE)) {
		type=NOUVEAU_MEM_FB;
		if (!(flags&NOUVEAU_MEM_MAPPED)) {
			block = alloc_block(dev_priv->fb_nomap_heap, size, alignment, filp);
			if (block) goto alloc_ok;
		}
		block = alloc_block(dev_priv->fb_heap, size, alignment, filp);
		if (block) goto alloc_ok;	
	}
	if (flags&NOUVEAU_MEM_AGP_ACCEPTABLE) {
		type=NOUVEAU_MEM_AGP;
		block = alloc_block(dev_priv->agp_heap, size, alignment, filp);
		if (block) goto alloc_ok;
	}

	return NULL;

alloc_ok:
	block->flags=type;

	if (flags&NOUVEAU_MEM_MAPPED)
	{
		int ret;
		block->flags|=NOUVEAU_MEM_MAPPED;

		if (type == NOUVEAU_MEM_AGP)
			ret = drm_addmap(dev, block->start - dev->agp->base, block->size, 
					_DRM_AGP, 0, &block->map);
		else
			ret = drm_addmap(dev, block->start, block->size,
					_DRM_FRAME_BUFFER, 0, &block->map);
		if (ret) { 
			free_block(block);
			return NULL;
		}
	}

	DRM_INFO("allocated 0x%llx\n", block->start);
	return block;
}

void nouveau_mem_free(struct drm_device* dev, struct mem_block* block)
{
	DRM_INFO("freeing 0x%llx\n", block->start);
	if (block->flags&NOUVEAU_MEM_MAPPED)
		drm_rmmap(dev, block->map);
	free_block(block);
}

static void
nouveau_instmem_determine_amount(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	/* Figure out how much instance memory we need */
	switch (dev_priv->card_type) {
	case NV_40:
		/* We'll want more instance memory than this on some NV4x cards.
		 * There's a 16MB aperture to play with that maps onto the end
		 * of vram.  For now, only reserve a small piece until we know
		 * more about what each chipset requires.
		 */
		dev_priv->ramin_size = (1*1024* 1024);
		break;
	default:
		/*XXX: what *are* the limits on <NV40 cards?, and does RAMIN
		 *     exist in vram on those cards as well?
		 */
		dev_priv->ramin_size = (512*1024);
		break;
	}
	DRM_DEBUG("RAMIN size: %dKiB\n", dev_priv->ramin_size>>10);

	/* Clear all of it, except the BIOS image that's in the first 64KiB */
	if (dev_priv->ramin) {
		for (i=(64*1024); i<dev_priv->ramin_size; i+=4)
			DRM_WRITE32(dev_priv->ramin, i, 0x00000000);
	} else {
		for (i=(64*1024); i<dev_priv->ramin_size; i+=4)
			DRM_WRITE32(dev_priv->mmio, NV_RAMIN + i, 0x00000000);
	}
}

static void
nouveau_instmem_configure_fixed_tables(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/* FIFO hash table (RAMHT)
	 *   use 4k hash table at RAMIN+0x10000
	 *   TODO: extend the hash table
	 */
	dev_priv->ramht_offset = 0x10000;
	dev_priv->ramht_bits   = 9;
	dev_priv->ramht_size   = (1 << dev_priv->ramht_bits);
	DRM_DEBUG("RAMHT offset=0x%x, size=%d\n", dev_priv->ramht_offset,
						  dev_priv->ramht_size);

	/* FIFO runout table (RAMRO) - 512k at 0x11200 */
	dev_priv->ramro_offset = 0x11200;
	dev_priv->ramro_size   = 512;
	DRM_DEBUG("RAMRO offset=0x%x, size=%d\n", dev_priv->ramro_offset,
						  dev_priv->ramro_size);

	/* FIFO context table (RAMFC)
	 *   NV40  : Not sure exactly how to position RAMFC on some cards,
	 *           0x30002 seems to position it at RAMIN+0x20000 on these
	 *           cards.  RAMFC is 4kb (32 fifos, 128byte entries).
	 *   Others: Position RAMFC at RAMIN+0x11400
	 */
	switch(dev_priv->card_type)
	{
		case NV_50:
		case NV_40:
		case NV_44:
			dev_priv->ramfc_offset = 0x20000;
			dev_priv->ramfc_size   = nouveau_fifo_number(dev) *
				nouveau_fifo_ctx_size(dev);
			break;
		case NV_30:
		case NV_20:
		case NV_17:
		case NV_10:
		case NV_04:
		case NV_03:
		default:
			dev_priv->ramfc_offset = 0x11400;
			dev_priv->ramfc_size   = nouveau_fifo_number(dev) *
				nouveau_fifo_ctx_size(dev);
			break;
	}
	DRM_DEBUG("RAMFC offset=0x%x, size=%d\n", dev_priv->ramfc_offset,
						  dev_priv->ramfc_size);
}

int nouveau_instmem_init(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t offset;
	int ret = 0;

	nouveau_instmem_determine_amount(dev);
	nouveau_instmem_configure_fixed_tables(dev);

	/* Create a heap to manage RAMIN allocations, we don't allocate
	 * the space that was reserved for RAMHT/FC/RO.
	 */
	offset = dev_priv->ramfc_offset + dev_priv->ramfc_size;
	ret = init_heap(&dev_priv->ramin_heap,
			 offset, dev_priv->ramin_size - offset);
	if (ret) {
		dev_priv->ramin_heap = NULL;
		DRM_ERROR("Failed to init RAMIN heap\n");
	}

	return ret;
}

struct mem_block *nouveau_instmem_alloc(struct drm_device *dev,
					uint32_t size, uint32_t align)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct mem_block *block;

	if (!dev_priv->ramin_heap) {
		DRM_ERROR("instmem alloc called without init\n");
		return NULL;
	}

	block = alloc_block(dev_priv->ramin_heap, size, align, (DRMFILE)-2);
	if (block) {
		block->flags = NOUVEAU_MEM_INSTANCE;
		DRM_DEBUG("instance(size=%d, align=%d) alloc'd at 0x%08x\n",
				size, (1<<align), (uint32_t)block->start);
	}

	return block;
}

void nouveau_instmem_free(struct drm_device *dev, struct mem_block *block)
{
	if (dev && block) {
		free_block(block);
	}
}

uint32_t nouveau_instmem_r32(drm_nouveau_private_t *dev_priv,
			     struct mem_block *mem, int index)
{
	uint32_t ofs = (uint32_t)mem->start + (index<<2);

	if (dev_priv->ramin) {
#if defined(__powerpc__)
		return in_be32((void __iomem *)(dev_priv->ramin)->handle + ofs);
#else
		return DRM_READ32(dev_priv->ramin, ofs);
#endif
	} else {
		return NV_READ(NV_RAMIN+ofs);
	}
}

void nouveau_instmem_w32(drm_nouveau_private_t *dev_priv,
			 struct mem_block *mem, int index, uint32_t val)
{
	uint32_t ofs = (uint32_t)mem->start + (index<<2);

	if (dev_priv->ramin) {
#if defined(__powerpc__)
		out_be32((void __iomem *)(dev_priv->ramin)->handle + ofs, val);
#else
		DRM_WRITE32(dev_priv->ramin, ofs, val);
#endif
	} else {
		NV_WRITE(NV_RAMIN+ofs, val);
	}
}

/*
 * Ioctls
 */

int nouveau_ioctl_mem_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	drm_nouveau_mem_alloc_t alloc;
	struct mem_block *block;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(alloc, (drm_nouveau_mem_alloc_t __user *) data,
				 sizeof(alloc));

	block=nouveau_mem_alloc(dev, alloc.alignment, alloc.size, alloc.flags, filp);
	if (!block)
		return DRM_ERR(ENOMEM);
	alloc.region_offset=block->start;
	alloc.flags=block->flags;

	DRM_COPY_TO_USER_IOCTL((drm_nouveau_mem_alloc_t __user *) data, alloc, sizeof(alloc));

	return 0;
}

int nouveau_ioctl_mem_free(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	drm_nouveau_mem_free_t memfree;
	struct mem_block *block;

	DRM_COPY_FROM_USER_IOCTL(memfree, (drm_nouveau_mem_free_t __user *) data,
				 sizeof(memfree));

	block=NULL;
	if (memfree.flags&NOUVEAU_MEM_FB)
		block = find_block(dev_priv->fb_heap, memfree.region_offset);
	else if (memfree.flags&NOUVEAU_MEM_AGP)
		block = find_block(dev_priv->agp_heap, memfree.region_offset);
	if (!block)
		return DRM_ERR(EFAULT);
	if (block->filp != filp)
		return DRM_ERR(EPERM);

	nouveau_mem_free(dev, block);
	return 0;
}



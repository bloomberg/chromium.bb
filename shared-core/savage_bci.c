/* savage_bci.c -- BCI support for Savage
 *
 * Copyright 2004  Felix Kuehling
 * All Rights Reserved.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL FELIX KUEHLING BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "drmP.h"
#include "savage_drm.h"
#include "savage_drv.h"

/* Need a long timeout for shadow status updates can take a while
 * and so can waiting for events when the queue is full. */
#define SAVAGE_DEFAULT_USEC_TIMEOUT	1000000 /* 1s */
#define SAVAGE_EVENT_USEC_TIMEOUT	5000000 /* 5s */
#define SAVAGE_FREELIST_DEBUG		0

static int
savage_bci_wait_fifo_shadow(drm_savage_private_t *dev_priv, unsigned int n)
{
	uint32_t mask = dev_priv->status_used_mask;
	uint32_t threshold = dev_priv->bci_threshold_hi;
	uint32_t status;
	int i;

#if SAVAGE_BCI_DEBUG
	if (n > dev_priv->cob_size + SAVAGE_BCI_FIFO_SIZE - threshold)
		DRM_ERROR("Trying to emit %d words "
			  "(more than guaranteed space in COB)\n", n);
#endif

	for (i = 0; i < SAVAGE_DEFAULT_USEC_TIMEOUT; i++) {
		status = dev_priv->status_ptr[0];
		if ((status & mask) < threshold)
			return 0;
		DRM_UDELAY(1);
	}

#if SAVAGE_BCI_DEBUG
	DRM_ERROR("failed!\n");
	DRM_INFO("   status=0x%08x, threshold=0x%08x\n", status, threshold);
#endif
	return DRM_ERR(EBUSY);
}

static int
savage_bci_wait_fifo_s3d(drm_savage_private_t *dev_priv, unsigned int n)
{
	uint32_t maxUsed = dev_priv->cob_size + SAVAGE_BCI_FIFO_SIZE - n;
	uint32_t status;
	int i;

	for (i = 0; i < SAVAGE_DEFAULT_USEC_TIMEOUT; i++) {
		status = SAVAGE_READ(SAVAGE_STATUS_WORD0);
		if ((status & SAVAGE_FIFO_USED_MASK_S3D) <= maxUsed)
			return 0;
		DRM_UDELAY(1);
	}

#if SAVAGE_BCI_DEBUG
	DRM_ERROR("failed!\n");
	DRM_INFO("   status=0x%08x\n", status);
#endif
	return DRM_ERR(EBUSY);
}

static int
savage_bci_wait_fifo_s4(drm_savage_private_t *dev_priv, unsigned int n)
{
	uint32_t maxUsed = dev_priv->cob_size + SAVAGE_BCI_FIFO_SIZE - n;
	uint32_t status;
	int i;

	for (i = 0; i < SAVAGE_DEFAULT_USEC_TIMEOUT; i++) {
		status = SAVAGE_READ(SAVAGE_ALT_STATUS_WORD0);
		if ((status & SAVAGE_FIFO_USED_MASK_S4) <= maxUsed)
			return 0;
		DRM_UDELAY(1);
	}

#if SAVAGE_BCI_DEBUG
	DRM_ERROR("failed!\n");
	DRM_INFO("   status=0x%08x\n", status);
#endif
	return DRM_ERR(EBUSY);
}

/*
 * Waiting for events.
 *
 * The BIOSresets the event tag to 0 on mode changes. Therefore we
 * never emit 0 to the event tag. If we find a 0 event tag we know the
 * BIOS stomped on it and return success assuming that the BIOS waited
 * for engine idle.
 *
 * Note: if the Xserver uses the event tag it has to follow the same
 * rule. Otherwise there may be glitches every 2^16 events.
 */
static int
savage_bci_wait_event_shadow(drm_savage_private_t *dev_priv, uint16_t e)
{
	uint32_t status;
	int i;

	for (i = 0; i < SAVAGE_EVENT_USEC_TIMEOUT; i++) {
		status = dev_priv->status_ptr[1];
		if ((((status & 0xffff) - e) & 0xffff) <= 0x7fff ||
		    (status & 0xffff) == 0)
			return 0;
		DRM_UDELAY(1);
	}

#if SAVAGE_BCI_DEBUG
	DRM_ERROR("failed!\n");
	DRM_INFO("   status=0x%08x, e=0x%04x\n", status, e);
#endif

	return DRM_ERR(EBUSY);
}

static int
savage_bci_wait_event_reg(drm_savage_private_t *dev_priv, uint16_t e)
{
	uint32_t status;
	int i;

	for (i = 0; i < SAVAGE_EVENT_USEC_TIMEOUT; i++) {
		status = SAVAGE_READ(SAVAGE_STATUS_WORD1);
		if ((((status & 0xffff) - e) & 0xffff) <= 0x7fff ||
		    (status & 0xffff) == 0)
			return 0;
		DRM_UDELAY(1);
	}

#if SAVAGE_BCI_DEBUG
	DRM_ERROR("failed!\n");
	DRM_INFO("   status=0x%08x, e=0x%04x\n", status, e);
#endif

	return DRM_ERR(EBUSY);
}

uint16_t savage_bci_emit_event(drm_savage_private_t *dev_priv,
			       unsigned int flags)
{
	BCI_LOCALS;
	uint16_t count;

	if (dev_priv->status_ptr) {
		/* coordinate with Xserver */
		count = dev_priv->status_ptr[1023];
		if (count < dev_priv->event_counter)
			dev_priv->event_wrap++;
	} else {
		count = dev_priv->event_counter;
	}
	count = (count + 1) & 0xffff;
	if (count == 0) {
		count++; /* See the comment above savage_wait_event_*. */
		dev_priv->event_wrap++;
	}
	dev_priv->event_counter = count;
	if (dev_priv->status_ptr)
		dev_priv->status_ptr[1023] = (uint32_t)count;

	if ((flags & (SAVAGE_WAIT_2D | SAVAGE_WAIT_3D))) {
		unsigned int wait_cmd = BCI_CMD_WAIT;
		if ((flags & SAVAGE_WAIT_2D))
			wait_cmd |= BCI_CMD_WAIT_2D;
		if ((flags & SAVAGE_WAIT_3D))
			wait_cmd |= BCI_CMD_WAIT_3D;
		BEGIN_BCI(2);
		BCI_WRITE(wait_cmd);
	} else {
		BEGIN_BCI(1);
	}
	BCI_WRITE(BCI_CMD_UPDATE_EVENT_TAG | (uint32_t)count);

	return count;
}

/*
 * Freelist management
 */
static int savage_freelist_init(drm_device_t *dev)
{
	drm_savage_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_savage_buf_priv_t *entry;
	int i;
	DRM_DEBUG("count=%d\n", dma->buf_count);

	dev_priv->head.next = &dev_priv->tail;
	dev_priv->head.prev = NULL;
	dev_priv->head.buf = NULL;

	dev_priv->tail.next = NULL;
	dev_priv->tail.prev = &dev_priv->head;
	dev_priv->tail.buf = NULL;

	for (i = 0; i < dma->buf_count; i++) {
		buf = dma->buflist[i];
		entry = buf->dev_private;

		SET_AGE(&entry->age, 0, 0);
		entry->buf = buf;

		entry->next = dev_priv->head.next;
		entry->prev = &dev_priv->head;
		dev_priv->head.next->prev = entry;
		dev_priv->head.next = entry;
	}

	return 0;
}

static drm_buf_t *savage_freelist_get(drm_device_t *dev)
{
	drm_savage_private_t *dev_priv = dev->dev_private;
	drm_savage_buf_priv_t *tail = dev_priv->tail.prev;
	uint16_t event;
	unsigned int wrap;
	DRM_DEBUG("\n");

	UPDATE_EVENT_COUNTER();
	if (dev_priv->status_ptr)
		event = dev_priv->status_ptr[1] & 0xffff;
	else
		event = SAVAGE_READ(SAVAGE_STATUS_WORD1) & 0xffff;
	wrap = dev_priv->event_wrap;

	DRM_DEBUG("   tail=0x%04x %d\n", tail->age.event, tail->age.wrap);
	DRM_DEBUG("   head=0x%04x %d\n", event, wrap);

	if (tail->buf && (TEST_AGE(&tail->age, event, wrap) || event == 0)) {
		drm_savage_buf_priv_t *next = tail->next;
		drm_savage_buf_priv_t *prev = tail->prev;
		prev->next = next;
		next->prev = prev;
		tail->next = tail->prev = NULL;
		return tail->buf;
	}

	DRM_DEBUG("returning NULL, tail->buf=%p!\n", tail->buf);
	return NULL;
}

void savage_freelist_put(drm_device_t *dev, drm_buf_t *buf)
{
	drm_savage_private_t *dev_priv = dev->dev_private;
	drm_savage_buf_priv_t *entry = buf->dev_private, *prev, *next;

	DRM_DEBUG("age=0x%04x wrap=%d\n", entry->age.event, entry->age.wrap);

	if (entry->next != NULL || entry->prev != NULL) {
		DRM_ERROR("entry already on freelist.\n");
		return;
	}

	prev = &dev_priv->head;
	next = prev->next;
	prev->next = entry;
	next->prev = entry;
	entry->prev = prev;
	entry->next = next;
}

/*
 * Initalize permanent mappings. On Savage4 and SavageIX the alignment
 * and size of the aperture is not suitable for automatic MTRR setup
 * in drm_initmap. Therefore we do it manually before the maps are
 * initialized. We also need to take care of deleting the MTRRs in
 * postcleanup.
 *
 * FIXME: this is linux-specific
 */
int savage_preinit(drm_device_t *dev, unsigned long chipset)
{
	drm_savage_private_t *dev_priv;
	unsigned long mmio_base, fb_base, fb_size, aperture_base;
	unsigned int fb_rsrc, aper_rsrc;
	int ret = 0;

	dev_priv = drm_alloc(sizeof(drm_savage_private_t), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return DRM_ERR(ENOMEM);

	memset(dev_priv, 0, sizeof(drm_savage_private_t));
	dev->dev_private = (void *)dev_priv;
	dev_priv->chipset = (enum savage_family)chipset;

	dev_priv->mtrr[0].handle = -1;
	dev_priv->mtrr[1].handle = -1;
	dev_priv->mtrr[2].handle = -1;
	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		fb_rsrc = 0;
		fb_base = drm_get_resource_start(dev, 0);
		fb_size = SAVAGE_FB_SIZE_S3;
		mmio_base = fb_base + SAVAGE_FB_SIZE_S3;
		aper_rsrc = 0;
		aperture_base = fb_base + SAVAGE_APERTURE_OFFSET;
		/* this should always be true */
		if (drm_get_resource_len(dev, 0) == 0x08000000) {
			/* Don't make MMIO write-cobining! We need 3
			 * MTRRs. */
			dev_priv->mtrr[0].base = fb_base;
			dev_priv->mtrr[0].size = 0x01000000;
			dev_priv->mtrr[0].handle = mtrr_add(
				dev_priv->mtrr[0].base, dev_priv->mtrr[0].size,
				MTRR_TYPE_WRCOMB, 1);
			dev_priv->mtrr[1].base = fb_base+0x02000000;
			dev_priv->mtrr[1].size = 0x02000000;
			dev_priv->mtrr[1].handle = mtrr_add(
				dev_priv->mtrr[1].base, dev_priv->mtrr[1].size,
				MTRR_TYPE_WRCOMB, 1);
			dev_priv->mtrr[2].base = fb_base+0x04000000;
			dev_priv->mtrr[2].size = 0x04000000;
			dev_priv->mtrr[2].handle = mtrr_add(
				dev_priv->mtrr[2].base, dev_priv->mtrr[2].size,
				MTRR_TYPE_WRCOMB, 1);
		} else {
			DRM_ERROR("strange pci_resource_len %08lx\n",
				  drm_get_resource_len(dev, 0));
		}
	} else if (chipset != S3_SUPERSAVAGE && chipset != S3_SAVAGE2000) {
		mmio_base = drm_get_resource_start(dev, 0);
		fb_rsrc = 1;
		fb_base = drm_get_resource_start(dev, 1);
		fb_size = SAVAGE_FB_SIZE_S4;
		aper_rsrc = 1;
		aperture_base = fb_base + SAVAGE_APERTURE_OFFSET;
		/* this should always be true */
		if (drm_get_resource_len(dev, 1) == 0x08000000) {
			/* Can use one MTRR to cover both fb and
			 * aperture. */
			dev_priv->mtrr[0].base = fb_base;
			dev_priv->mtrr[0].size = 0x08000000;
			dev_priv->mtrr[0].handle = mtrr_add(
				dev_priv->mtrr[0].base, dev_priv->mtrr[0].size,
				MTRR_TYPE_WRCOMB, 1);
		} else {
			DRM_ERROR("strange pci_resource_len %08lx\n",
				  drm_get_resource_len(dev, 1));
		}
	} else {
		mmio_base = drm_get_resource_start(dev, 0);
		fb_rsrc = 1;
		fb_base = drm_get_resource_start(dev, 1);
		fb_size = drm_get_resource_len(dev, 1);
		aper_rsrc = 2;
		aperture_base = drm_get_resource_start(dev, 2);
		/* Automatic MTRR setup will do the right thing. */
	}

	if ((ret = drm_initmap(dev, mmio_base, SAVAGE_MMIO_SIZE, 0,
			       _DRM_REGISTERS, 0)))
		return ret;
	if (!(dev_priv->mmio = drm_core_findmap (dev, mmio_base)))
		return DRM_ERR(ENOMEM);

	if ((ret = drm_initmap(dev, fb_base, fb_size, fb_rsrc,
			       _DRM_FRAME_BUFFER, _DRM_WRITE_COMBINING)))
		return ret;
	if (!(dev_priv->fb = drm_core_findmap (dev, fb_base)))
		return DRM_ERR(ENOMEM);

	if ((ret = drm_initmap(dev, aperture_base, SAVAGE_APERTURE_SIZE,
			       aper_rsrc,
			       _DRM_FRAME_BUFFER, _DRM_WRITE_COMBINING)))
		return ret;
	if (!(dev_priv->aperture = drm_core_findmap (dev, aperture_base)))
		return DRM_ERR(ENOMEM);

	return ret;
}

/*
 * Delete MTRRs and free device-private data.
 */
int savage_postcleanup(drm_device_t *dev)
{
	drm_savage_private_t *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < 3; ++i)
		if (dev_priv->mtrr[i].handle >= 0)
			mtrr_del(dev_priv->mtrr[i].handle,
				 dev_priv->mtrr[i].base,
				 dev_priv->mtrr[i].size);

	drm_free(dev_priv, sizeof(drm_savage_private_t), DRM_MEM_DRIVER);

	return 0;
}

static int savage_do_init_bci(drm_device_t *dev, drm_savage_init_t *init)
{
	drm_savage_private_t *dev_priv = dev->dev_private;

	if (init->fb_bpp != 16 && init->fb_bpp != 32) {
		DRM_ERROR("invalid frame buffer bpp %d!\n", init->fb_bpp);
		return DRM_ERR(EINVAL);
	}
	if (init->depth_bpp != 16 && init->depth_bpp != 32) {
		DRM_ERROR("invalid depth buffer bpp %d!\n", init->fb_bpp);
		return DRM_ERR(EINVAL);
	}
	if (init->dma_type != SAVAGE_DMA_AGP &&
	    init->dma_type != SAVAGE_DMA_PCI) {
		DRM_ERROR("invalid dma memory type %d!\n", init->dma_type);
		return DRM_ERR(EINVAL);
	}

	dev_priv->cob_size = init->cob_size;
	dev_priv->bci_threshold_lo = init->bci_threshold_lo;
	dev_priv->bci_threshold_hi = init->bci_threshold_hi;
	dev_priv->dma_type = init->dma_type;

	dev_priv->fb_bpp = init->fb_bpp;
	dev_priv->front_offset = init->front_offset;
	dev_priv->front_pitch = init->front_pitch;
	dev_priv->back_offset = init->back_offset;
	dev_priv->back_pitch = init->back_pitch;
	dev_priv->depth_bpp = init->depth_bpp;
	dev_priv->depth_offset = init->depth_offset;
	dev_priv->depth_pitch = init->depth_pitch;

	dev_priv->texture_offset = init->texture_offset;
	dev_priv->texture_size = init->texture_size;

	DRM_GETSAREA();
	if (!dev_priv->sarea) {
		DRM_ERROR("could not find sarea!\n");
		savage_do_cleanup_bci(dev);
		return DRM_ERR(EINVAL);
	}
	if (init->status_offset != 0) {
		dev_priv->status = drm_core_findmap(dev, init->status_offset);
		if (!dev_priv->status) {
			DRM_ERROR("could not find shadow status region!\n");
			savage_do_cleanup_bci(dev);
			return DRM_ERR(EINVAL);
		}
	} else {
		dev_priv->status = NULL;
	}
	if (dev_priv->dma_type == SAVAGE_DMA_AGP) {
		dev->agp_buffer_map = drm_core_findmap(dev,
						       init->buffers_offset);
		if (!dev->agp_buffer_map) {
			DRM_ERROR("could not find dma buffer region!\n");
			savage_do_cleanup_bci(dev);
			return DRM_ERR(EINVAL);
		}
	}
	if (init->agp_textures_offset) {
		dev_priv->agp_textures =
			drm_core_findmap(dev, init->agp_textures_offset);
		if (!dev_priv->agp_textures) {
			DRM_ERROR("could not find agp texture region!\n");
			savage_do_cleanup_bci(dev);
			return DRM_ERR(EINVAL);
		}
	} else {
		dev_priv->agp_textures = NULL;
	}
	if (0 && !S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		/* command DMA not implemented yet */
		dev_priv->cmd_dma = drm_core_findmap(dev, init->cmd_dma_offset);
		if (!dev_priv->cmd_dma) {
			DRM_ERROR("could not find command DMA region!\n");
			savage_do_cleanup_bci(dev);
			return DRM_ERR(EINVAL);
		}
	} else {
		dev_priv->cmd_dma = NULL;
	}

	if (dev_priv->cmd_dma && dev_priv->dma_type == SAVAGE_DMA_AGP) {
		drm_core_ioremap(dev_priv->cmd_dma, dev);
		if (!dev_priv->cmd_dma->handle) {
			DRM_ERROR("failed to ioremap command DMA region!\n");
			savage_do_cleanup_bci(dev);
			return DRM_ERR(ENOMEM);
		}
	}

	dev_priv->sarea_priv =
		(drm_savage_sarea_t *)((uint8_t *)dev_priv->sarea->handle +
				       init->sarea_priv_offset);

	/* setup bitmap descriptors */
	{
		unsigned int color_tile_format;
		unsigned int depth_tile_format;
		unsigned int front_stride, back_stride, depth_stride;
		if (dev_priv->chipset <= S3_SAVAGE4) {
			color_tile_format = dev_priv->fb_bpp == 16 ?
				SAVAGE_BD_TILE_16BPP : SAVAGE_BD_TILE_32BPP;
			depth_tile_format = dev_priv->depth_bpp == 16 ?
				SAVAGE_BD_TILE_16BPP : SAVAGE_BD_TILE_32BPP;
		} else {
			color_tile_format = SAVAGE_BD_TILE_DEST;
			depth_tile_format = SAVAGE_BD_TILE_DEST;
		}
		front_stride = dev_priv->front_pitch / (dev_priv->fb_bpp/8);
		back_stride  = dev_priv-> back_pitch / (dev_priv->fb_bpp/8);
		depth_stride = dev_priv->depth_pitch / (dev_priv->depth_bpp/8);

		dev_priv->front_bd = front_stride | SAVAGE_BD_BW_DISABLE |
			(dev_priv->fb_bpp << SAVAGE_BD_BPP_SHIFT) |
			(color_tile_format << SAVAGE_BD_TILE_SHIFT);

		dev_priv-> back_bd =  back_stride | SAVAGE_BD_BW_DISABLE |
			(dev_priv->fb_bpp << SAVAGE_BD_BPP_SHIFT) |
			(color_tile_format << SAVAGE_BD_TILE_SHIFT);

		dev_priv->depth_bd = depth_stride | SAVAGE_BD_BW_DISABLE |
			(dev_priv->depth_bpp << SAVAGE_BD_BPP_SHIFT) |
			(depth_tile_format << SAVAGE_BD_TILE_SHIFT);
	}

	/* setup status and bci ptr */
	dev_priv->event_counter = 0;
	dev_priv->event_wrap = 0;
	dev_priv->bci_ptr = (volatile uint32_t *)
	    ((uint8_t *)dev_priv->mmio->handle + SAVAGE_BCI_OFFSET);
	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		dev_priv->status_used_mask = SAVAGE_FIFO_USED_MASK_S3D;
	} else {
		dev_priv->status_used_mask = SAVAGE_FIFO_USED_MASK_S4;
	}
	if (dev_priv->status != NULL) {
		dev_priv->status_ptr =
			(volatile uint32_t *)dev_priv->status->handle;
		dev_priv->wait_fifo = savage_bci_wait_fifo_shadow;
		dev_priv->wait_evnt = savage_bci_wait_event_shadow;
		dev_priv->status_ptr[1023] = dev_priv->event_counter;
	} else {
		dev_priv->status_ptr = NULL;
		if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
			dev_priv->wait_fifo = savage_bci_wait_fifo_s3d;
		} else {
			dev_priv->wait_fifo = savage_bci_wait_fifo_s4;
		}
		dev_priv->wait_evnt = savage_bci_wait_event_reg;
	}

	/* cliprect functions */
	if (S3_SAVAGE3D_SERIES(dev_priv->chipset))
		dev_priv->emit_clip_rect = savage_emit_clip_rect_s3d;
	else
		dev_priv->emit_clip_rect = savage_emit_clip_rect_s4;

	if (savage_freelist_init(dev) < 0) {
		DRM_ERROR("could not initialize freelist\n");
		savage_do_cleanup_bci(dev);
		return DRM_ERR(ENOMEM);
	}

	return 0;
}

int savage_do_cleanup_bci(drm_device_t *dev)
{
	drm_savage_private_t *dev_priv = dev->dev_private;

	if (dev_priv->cmd_dma && dev_priv->dma_type == SAVAGE_DMA_AGP)
		drm_core_ioremapfree(dev_priv->cmd_dma, dev);

	return 0;
}

int savage_bci_init(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_savage_init_t init;

	LOCK_TEST_WITH_RETURN(dev, filp);

	DRM_COPY_FROM_USER_IOCTL(init, (drm_savage_init_t __user *)data,
				 sizeof(init));

	switch (init.func) {
	case SAVAGE_INIT_BCI:
		return savage_do_init_bci(dev, &init);
	case SAVAGE_CLEANUP_BCI:
		return savage_do_cleanup_bci(dev);
	}

	return DRM_ERR(EINVAL);
}

int savage_bci_event_emit(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_savage_private_t *dev_priv = dev->dev_private;
	drm_savage_event_emit_t event;

	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, filp);

	DRM_COPY_FROM_USER_IOCTL(event, (drm_savage_event_emit_t __user *)data,
				 sizeof(event));

	event.count = savage_bci_emit_event(dev_priv, event.flags);
	DRM_COPY_TO_USER_IOCTL(&((drm_savage_event_emit_t __user *)data)->count,
			       event.count, sizeof(event.count));
	return 0;
}

int savage_bci_event_wait(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_savage_private_t *dev_priv = dev->dev_private;
	drm_savage_event_wait_t event;

	DRM_DEBUG("\n");

	DRM_COPY_FROM_USER_IOCTL(event, (drm_savage_event_wait_t __user *)data,
				 sizeof(event));

	if (event.count > 0xffff)
		return DRM_ERR(EINVAL);

	return dev_priv->wait_evnt(dev_priv, event.count);
}

/*
 * DMA buffer management
 */

static int savage_bci_get_buffers(DRMFILE filp, drm_device_t *dev, drm_dma_t *d)
{
	drm_buf_t *buf;
	int i;

	for (i = d->granted_count; i < d->request_count; i++) {
		buf = savage_freelist_get(dev);
		if (!buf)
			return DRM_ERR(EAGAIN);

		buf->filp = filp;

		if (DRM_COPY_TO_USER(&d->request_indices[i],
				     &buf->idx, sizeof(buf->idx)))
			return DRM_ERR(EFAULT);
		if (DRM_COPY_TO_USER(&d->request_sizes[i],
				     &buf->total, sizeof(buf->total)))
			return DRM_ERR(EFAULT);

		d->granted_count++;
	}
	return 0;
}

int savage_bci_buffers(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_dma_t d;
	int ret = 0;

	LOCK_TEST_WITH_RETURN(dev, filp);

	DRM_COPY_FROM_USER_IOCTL(d, (drm_dma_t __user *)data, sizeof(d));

	/* Please don't send us buffers.
	 */
	if (d.send_count != 0) {
		DRM_ERROR("Process %d trying to send %d buffers via drmDMA\n",
			  DRM_CURRENTPID, d.send_count);
		return DRM_ERR(EINVAL);
	}

	/* We'll send you buffers.
	 */
	if (d.request_count < 0 || d.request_count > dma->buf_count) {
		DRM_ERROR("Process %d trying to get %d buffers (of %d max)\n",
			  DRM_CURRENTPID, d.request_count, dma->buf_count);
		return DRM_ERR(EINVAL);
	}

	d.granted_count = 0;

	if (d.request_count) {
		ret = savage_bci_get_buffers(filp, dev, &d);
	}

	DRM_COPY_TO_USER_IOCTL((drm_dma_t __user *)data, d, sizeof(d));

	return ret;
}

void savage_reclaim_buffers(drm_device_t *dev, DRMFILE filp) {
	drm_device_dma_t *dma = dev->dma;
	drm_savage_private_t *dev_priv = dev->dev_private;
	int i;

	if (!dma)
		return;
	if (!dev_priv)
		return;
	if (!dma->buflist)
		return;

	/*i830_flush_queue(dev);*/

	for (i = 0; i < dma->buf_count; i++) {
		drm_buf_t *buf = dma->buflist[i];
		drm_savage_buf_priv_t *buf_priv = buf->dev_private;

		if (buf->filp == filp && buf_priv &&
		    buf_priv->next == NULL && buf_priv->prev == NULL) {
			uint16_t event;
			DRM_DEBUG("reclaimed from client\n");
			event = savage_bci_emit_event(dev_priv, SAVAGE_WAIT_3D);
			SET_AGE(&buf_priv->age, event, dev_priv->event_wrap);
			savage_freelist_put(dev, buf);
		}
	}

	drm_core_reclaim_buffers(dev, filp);
}

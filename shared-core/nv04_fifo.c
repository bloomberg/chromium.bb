/*
 * Copyright (C) 2007 Ben Skeggs.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"

#define NV04_RAMFC (NV_RAMIN + dev_priv->ramfc_offset)
#define RAMFC_WR(offset, val) NV_WRITE(fifoctx + NV04_RAMFC_##offset, (val))
#define RAMFC_RD(offset)      NV_READ(fifoctx + NV04_RAMFC_##offset)
#define NV04_FIFO_CONTEXT_SIZE 32

int
nv04_fifo_create_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	struct nouveau_object *pb = chan->cmdbuf_obj;
	int fifoctx = NV04_RAMFC + (channel * NV04_FIFO_CONTEXT_SIZE);
	int i;

	if (!pb || !pb->instance)
		return DRM_ERR(EINVAL);

	/* Clear RAMFC */
	for (i=0; i<NV04_FIFO_CONTEXT_SIZE; i+=4)
		NV_WRITE(fifoctx + i, 0);
	
	/* Setup initial state */
	RAMFC_WR(DMA_PUT, chan->pushbuf_base);
	RAMFC_WR(DMA_GET, chan->pushbuf_base);
	RAMFC_WR(DMA_INSTANCE, nouveau_chip_instance_get(dev, pb->instance));
	/* NOTE: nvidia use TRIG_128/SIZE_128/MAX_REQS_8 */
	RAMFC_WR(DMA_FETCH, (NV_PFIFO_CACHE1_DMA_FETCH_TRIG_112_BYTES |
			     NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
			     NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_4 |
#ifdef __BIG_ENDIAN
			     NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
			     0));
	return 0;
}

void
nv04_fifo_destroy_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	int i;

	fifoctx = NV04_RAMFC + (channel * NV04_FIFO_CONTEXT_SIZE);
	for (i=0; i<NV04_FIFO_CONTEXT_SIZE; i+=4)
		NV_WRITE(fifoctx + i, 0);
}

int
nv04_fifo_load_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int fifoctx = NV04_RAMFC + (channel * NV04_FIFO_CONTEXT_SIZE);
	uint32_t tmp;

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUT, RAMFC_RD(DMA_PUT));
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_GET, RAMFC_RD(DMA_GET));
	
	tmp = RAMFC_RD(DMA_INSTANCE);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_INSTANCE, tmp & 0xFFFF);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_DCOUNT, tmp >> 16);
	
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_STATE, RAMFC_RD(DMA_STATE));
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_FETCH, RAMFC_RD(DMA_FETCH));
	NV_WRITE(NV04_PFIFO_CACHE1_ENGINE, RAMFC_RD(ENGINE));
	NV_WRITE(NV04_PFIFO_CACHE1_PULL1, RAMFC_RD(PULL1_ENGINE));

	/* Reset NV04_PFIFO_CACHE1_DMA_CTL_AT_INFO to INVALID */
	tmp = NV_READ(NV04_PFIFO_CACHE1_DMA_CTL) & ~(1<<31);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_CTL, tmp);

	return 0;
}

int
nv04_fifo_save_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int fifoctx = NV04_RAMFC + (channel * NV04_FIFO_CONTEXT_SIZE);
	uint32_t tmp;

	RAMFC_WR(DMA_PUT, NV04_PFIFO_CACHE1_DMA_PUT);
	RAMFC_WR(DMA_GET, NV04_PFIFO_CACHE1_DMA_GET);

	tmp  = NV_READ(NV04_PFIFO_CACHE1_DMA_DCOUNT) << 16;
	tmp |= NV_READ(NV04_PFIFO_CACHE1_DMA_INSTANCE);
	RAMFC_WR(DMA_INSTANCE, tmp);

	RAMFC_WR(DMA_STATE, NV_READ(NV04_PFIFO_CACHE1_DMA_STATE));
	RAMFC_WR(DMA_FETCH, NV_READ(NV04_PFIFO_CACHE1_DMA_FETCH));
	RAMFC_WR(ENGINE, NV_READ(NV04_PFIFO_CACHE1_ENGINE));
	RAMFC_WR(PULL1_ENGINE, NV_READ(NV04_PFIFO_CACHE1_PULL1));
	
	return 0;
}


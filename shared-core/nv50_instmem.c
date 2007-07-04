/*
 * Copyright (C) 2007 Ben Skeggs.
 *
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

typedef struct {
	uint32_t save1700[5]; /* 0x1700->0x1710 */
} nv50_instmem_priv;

#define NV50_INSTMEM_PAGE_SHIFT 12
#define NV50_INSTMEM_PAGE_SIZE  (1 << NV50_INSTMEM_PAGE_SHIFT)
#define NV50_INSTMEM_RSVD_SIZE	(64 * 1024)
#define NV50_INSTMEM_PT_SIZE(a)	(((a) >> 12) << 3)

int
nv50_instmem_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nv50_instmem_priv *priv;
	uint32_t rv, pt, pts, cb, cb0, cb1, unk, as;
	uint32_t i, v;
	int ret;

	priv = drm_calloc(1, sizeof(*priv), DRM_MEM_DRIVER);
	if (!priv)
		return DRM_ERR(ENOMEM);
	dev_priv->Engine.instmem.priv = priv;

	/* Save current state */
	for (i = 0x1700; i <= 0x1710; i+=4)
		priv->save1700[(i-0x1700)/4] = NV_READ(i);

	as  = dev_priv->ramin->size;
	rv  = nouveau_mem_fb_amount(dev) - (1*1024*1024);
	pt  = rv + 0xd0000;
	pts = NV50_INSTMEM_PT_SIZE(as);
	cb  = rv + 0xc8000;
	if ((dev_priv->chipset & 0xf0) != 0x50) {
		unk = cb + 0x4200;
		cb0 = cb + 0x4240;
		cb1 = cb + 0x278;
	} else {
		unk = cb + 0x5400;
		cb0 = cb + 0x5440;
		cb1 = cb + 0x1438;
	}

	DRM_DEBUG("PRAMIN config:\n");
	DRM_DEBUG(" Rsvd VRAM base: 0x%08x\n", rv);
	DRM_DEBUG("  Aperture size: %i MiB\n", as >> 20);
	DRM_DEBUG("        PT base: 0x%08x\n", pt);
	DRM_DEBUG("        PT size: %d KiB\n", pts >> 10);
	DRM_DEBUG("     BIOS image: 0x%08x\n", (NV_READ(0x619f04)&~0xff)<<8);
	DRM_DEBUG("    Config base: 0x%08x\n", cb);
	DRM_DEBUG(" ctxdma Config0: 0x%08x\n", cb0);
	DRM_DEBUG("        Config1: 0x%08x\n", cb1);

	/* Map first MiB of reserved vram into BAR0 PRAMIN aperture */
	NV_WRITE(0x1700, (rv>>16));
	/* Poke some regs.. */
	NV_WRITE(0x1704, (cb>>12));
	NV_WRITE(0x1710, (((unk-cb)>>4))|(1<<31));
	NV_WRITE(0x1704, (cb>>12)|(1<<30));

	/* CB0, some DMA object, NFI what it points at... Needed however,
	 * or the PRAMIN aperture doesn't operate as expected.
	 */
	NV_WRITE(NV_RAMIN + (cb0 - rv) + 0x00, 0x7fc00000);
	NV_WRITE(NV_RAMIN + (cb0 - rv) + 0x04, 0xe1ffffff);
	NV_WRITE(NV_RAMIN + (cb0 - rv) + 0x08, 0xe0000000);
	NV_WRITE(NV_RAMIN + (cb0 - rv) + 0x0c, 0x01000001);
	NV_WRITE(NV_RAMIN + (cb0 - rv) + 0x10, 0x00000000);
	NV_WRITE(NV_RAMIN + (cb0 - rv) + 0x14, 0x00000000);

	/* CB1, points at PRAMIN PT */
	NV_WRITE(NV_RAMIN + (cb1 - rv) + 0, pt | 0x63);
	NV_WRITE(NV_RAMIN + (cb1 - rv) + 4, 0x00000000);

	/* Zero PRAMIN page table */
	v  = NV_RAMIN + (pt - rv);
	for (i = v; i < v + pts; i += 8) {
		NV_WRITE(i + 0x00, 0x00000009);
		NV_WRITE(i + 0x04, 0x00000000);
	}

	/* Map page table into PRAMIN aperture */
	for (i = pt; i < pt + pts; i += 0x1000) {
		uint32_t pte = NV_RAMIN + (pt-rv) + (((i-pt) >> 12) << 3);
		DRM_DEBUG("PRAMIN PTE = 0x%08x @ 0x%08x\n", i, pte);
		NV_WRITE(pte + 0x00,      i | 1);
		NV_WRITE(pte + 0x04, 0x00000000);
	}

	/* Points at CB0 */
	NV_WRITE(0x170c, (((cb0 - cb)>>4)|(1<<31)));

	/* Confirm it all worked, should be able to read back the page table's
	 * PTEs from the PRAMIN BAR
	 */
	NV_WRITE(0x1700, pt >> 16);
	if (NV_READ(0x700000) != NV_RI32(0)) {
		DRM_ERROR("Failed to init PRAMIN page table\n");
		return DRM_ERR(EINVAL);
	}

	/* Create a heap to manage PRAMIN aperture allocations */
	ret = nouveau_mem_init_heap(&dev_priv->ramin_heap, pts, as-pts);
	if (ret) {
		DRM_ERROR("Failed to init PRAMIN heap\n");
		return DRM_ERR(ENOMEM);
	}
	DRM_DEBUG("NV50: PRAMIN setup ok\n");

	/* Don't alloc the last MiB of VRAM, probably too much, but be safe
	 * at least for now.
	 */
	dev_priv->ramin_rsvd_vram = 1*1024*1024;

	/*XXX: probably incorrect, but needed to make hash func "work" */
	dev_priv->ramht_offset = 0x10000;
	dev_priv->ramht_bits   = 9;
	dev_priv->ramht_size   = (1 << dev_priv->ramht_bits);
	return 0;
}

void
nv50_instmem_takedown(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nv50_instmem_priv *priv = dev_priv->Engine.instmem.priv;
	int i;

	if (!priv)
		return;

	/* Restore state from before init */
	for (i = 0x1700; i <= 0x1710; i+=4)
		NV_WRITE(i, priv->save1700[(i-0x1700)/4]);

	dev_priv->Engine.instmem.priv = NULL;
	drm_free(priv, sizeof(*priv), DRM_MEM_DRIVER);
}

int
nv50_instmem_populate(drm_device_t *dev, nouveau_gpuobj_t *gpuobj, uint32_t *sz)
{
	if (gpuobj->im_backing)
		return DRM_ERR(EINVAL);

	*sz = (*sz + (NV50_INSTMEM_PAGE_SIZE-1)) & ~(NV50_INSTMEM_PAGE_SIZE-1);
	if (*sz == 0)
		return DRM_ERR(EINVAL);

	gpuobj->im_backing = nouveau_mem_alloc(dev, NV50_INSTMEM_PAGE_SIZE,
					       *sz, NOUVEAU_MEM_FB,
					       (DRMFILE)-2);
	if (!gpuobj->im_backing) {
		DRM_ERROR("Couldn't allocate vram to back PRAMIN pages\n");
		return DRM_ERR(ENOMEM);
	}

	return 0;
}

void
nv50_instmem_clear(drm_device_t *dev, nouveau_gpuobj_t *gpuobj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	if (gpuobj && gpuobj->im_backing) {
		if (gpuobj->im_bound)
			dev_priv->Engine.instmem.unbind(dev, gpuobj);
		nouveau_mem_free(dev, gpuobj->im_backing);
		gpuobj->im_backing = NULL;
	}	
}

int
nv50_instmem_bind(drm_device_t *dev, nouveau_gpuobj_t *gpuobj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t pte, pte_end, vram;

	if (!gpuobj->im_backing || !gpuobj->im_pramin || gpuobj->im_bound)
		return DRM_ERR(EINVAL);

	DRM_DEBUG("st=0x%0llx sz=0x%0llx\n",
		  gpuobj->im_pramin->start, gpuobj->im_pramin->size);

	pte     = (gpuobj->im_pramin->start >> 12) << 3;
	pte_end = ((gpuobj->im_pramin->size >> 12) << 3) + pte;
	vram    = gpuobj->im_backing->start - dev_priv->fb_phys;

	if (pte == pte_end) {
		DRM_ERROR("WARNING: badness in bind() pte calc\n");
		pte_end++;
	}

	DRM_DEBUG("pramin=0x%llx, pte=%d, pte_end=%d\n",
		  gpuobj->im_pramin->start, pte, pte_end);
	DRM_DEBUG("first vram page: 0x%llx\n",
		  gpuobj->im_backing->start);

	while (pte < pte_end) {
		NV_WI32(pte + 0, vram | 1);
		NV_WI32(pte + 4, 0x00000000);

		pte += 8;
		vram += NV50_INSTMEM_PAGE_SIZE;
	}

	gpuobj->im_bound = 1;
	return 0;
}

int
nv50_instmem_unbind(drm_device_t *dev, nouveau_gpuobj_t *gpuobj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t pte, pte_end;

	if (gpuobj->im_bound == 0)
		return DRM_ERR(EINVAL);

	pte     = (gpuobj->im_pramin->start >> 12) << 3;
	pte_end = ((gpuobj->im_pramin->size >> 12) << 3) + pte;
	while (pte < pte_end) {
		NV_WI32(pte + 0, 0x00000000);
		NV_WI32(pte + 4, 0x00000000);
		pte += 8;
	}

	gpuobj->im_bound = 0;
	return 0;
}


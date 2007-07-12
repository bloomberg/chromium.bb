/*
 * Copyright (C) 2006 Ben Skeggs.
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

/*
 * Authors:
 *   Ben Skeggs <darktama@iinet.net.au>
 */

#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

/* NVidia uses context objects to drive drawing operations.

   Context objects can be selected into 8 subchannels in the FIFO,
   and then used via DMA command buffers.

   A context object is referenced by a user defined handle (CARD32). The HW
   looks up graphics objects in a hash table in the instance RAM.

   An entry in the hash table consists of 2 CARD32. The first CARD32 contains
   the handle, the second one a bitfield, that contains the address of the
   object in instance RAM.

   The format of the second CARD32 seems to be:

   NV4 to NV30:

   15: 0  instance_addr >> 4
   17:16  engine (here uses 1 = graphics)
   28:24  channel id (here uses 0)
   31	  valid (use 1)

   NV40:

   15: 0  instance_addr >> 4   (maybe 19-0)
   21:20  engine (here uses 1 = graphics)
   I'm unsure about the other bits, but using 0 seems to work.

   The key into the hash table depends on the object handle and channel id and
   is given as:
*/
static uint32_t
nouveau_ramht_hash_handle(drm_device_t *dev, int channel, uint32_t handle)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	uint32_t hash = 0;
	int i;

	for (i=32;i>0;i-=dev_priv->ramht_bits) {
		hash ^= (handle & ((1 << dev_priv->ramht_bits) - 1));
		handle >>= dev_priv->ramht_bits;
	}
	if (dev_priv->card_type < NV_50)
		hash ^= channel << (dev_priv->ramht_bits - 4);
	hash <<= 3;

	DRM_DEBUG("ch%d handle=0x%08x hash=0x%08x\n", channel, handle, hash);
	return hash;
}

static int
nouveau_ramht_entry_valid(drm_device_t *dev, nouveau_gpuobj_t *ramht,
			  uint32_t offset)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	uint32_t ctx = INSTANCE_RD(ramht, (offset + 4)/4);

	if (dev_priv->card_type < NV_40)
		return ((ctx & NV_RAMHT_CONTEXT_VALID) != 0);
	return (ctx != 0);
}

static int
nouveau_ramht_insert(drm_device_t* dev, nouveau_gpuobj_ref_t *ref)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[ref->channel];
	nouveau_gpuobj_t *ramht = chan->ramht ? chan->ramht->gpuobj : NULL;
	nouveau_gpuobj_t *gpuobj = ref->gpuobj;
	uint32_t ctx, co, ho;

	if (!ramht) {
		DRM_ERROR("No hash table!\n");
		return DRM_ERR(EINVAL);
	}

	if (dev_priv->card_type < NV_40) {
		ctx = NV_RAMHT_CONTEXT_VALID | (ref->instance >> 4) |
		      (ref->channel   << NV_RAMHT_CONTEXT_CHANNEL_SHIFT) |
		      (gpuobj->engine << NV_RAMHT_CONTEXT_ENGINE_SHIFT);
	} else
	if (dev_priv->card_type < NV_50) {
		ctx = (ref->instance >> 4) |
		      (ref->channel   << NV40_RAMHT_CONTEXT_CHANNEL_SHIFT) |
		      (gpuobj->engine << NV40_RAMHT_CONTEXT_ENGINE_SHIFT);
	} else {
		ctx = (ref->instance  >> 4) |
		      (gpuobj->engine << NV40_RAMHT_CONTEXT_ENGINE_SHIFT);
	}

	co = ho = nouveau_ramht_hash_handle(dev, ref->channel, ref->handle);
	do {
		if (!nouveau_ramht_entry_valid(dev, ramht, co)) {
			DRM_DEBUG("insert ch%d 0x%08x: h=0x%08x, c=0x%08x\n",
				  ref->channel, co, ref->handle, ctx);
			INSTANCE_WR(ramht, (co + 0)/4, ref->handle);
			INSTANCE_WR(ramht, (co + 4)/4, ctx);
			return 0;
		}
		DRM_DEBUG("collision ch%d 0x%08x: h=0x%08x\n",
			  ref->channel, co, INSTANCE_RD(ramht, co/4));

		co += 8;
		if (co >= dev_priv->ramht_size)
			co = 0;
	} while (co != ho);

	DRM_ERROR("RAMHT space exhausted. ch=%d\n", ref->channel);
	return DRM_ERR(ENOMEM);
}

static void
nouveau_ramht_remove(drm_device_t* dev, nouveau_gpuobj_ref_t *ref)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[ref->channel];
	nouveau_gpuobj_t *ramht = chan->ramht ? chan->ramht->gpuobj : NULL;
	uint32_t co, ho;

	if (!ramht) {
		DRM_ERROR("No hash table!\n");
		return;
	}

	co = ho = nouveau_ramht_hash_handle(dev, ref->channel, ref->handle);
	do {
		if (nouveau_ramht_entry_valid(dev, ramht, co) &&
		    (ref->handle == INSTANCE_RD(ramht, (co/4)))) {
			DRM_DEBUG("remove ch%d 0x%08x: h=0x%08x, c=0x%08x\n",
				  ref->channel, co, ref->handle,
				  INSTANCE_RD(ramht, (co + 4)));
			INSTANCE_WR(ramht, (co + 0)/4, 0x00000000);
			INSTANCE_WR(ramht, (co + 4)/4, 0x00000000);
			return;
		}

		co += 8;
		if (co >= dev_priv->ramht_size)
			co = 0;
	} while (co != ho);

	DRM_ERROR("RAMHT entry not found. ch=%d, handle=0x%08x\n",
		  ref->channel, ref->handle);
}

int
nouveau_gpuobj_new(drm_device_t *dev, int channel, int size, int align,
		   uint32_t flags, nouveau_gpuobj_t **gpuobj_ret)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_engine_func_t *engine = &dev_priv->Engine;
	struct nouveau_fifo *chan = NULL;
	nouveau_gpuobj_t *gpuobj;
	struct mem_block *pramin = NULL;
	int ret;

	DRM_DEBUG("ch%d size=%d align=%d flags=0x%08x\n",
		  channel, size, align, flags);

	if (!dev_priv || !gpuobj_ret || *gpuobj_ret != NULL)
		return DRM_ERR(EINVAL);

	if (channel >= 0) {
		if (channel > nouveau_fifo_number(dev))
			return DRM_ERR(EINVAL);
		chan = dev_priv->fifos[channel];
	}

	gpuobj = drm_calloc(1, sizeof(*gpuobj), DRM_MEM_DRIVER);
	if (!gpuobj)
		return DRM_ERR(ENOMEM);
	DRM_DEBUG("gpuobj %p\n", gpuobj);
	gpuobj->flags = flags;
	gpuobj->im_channel = channel;

	/* Choose between global instmem heap, and per-channel private
	 * instmem heap.  On <NV50 allow requests for private instmem
	 * to be satisfied from global heap if no per-channel area
	 * available.
	 */
	if (chan) {
		if (chan->ramin_heap) {
			DRM_DEBUG("private heap\n");
			pramin = chan->ramin_heap;
		} else
		if (dev_priv->card_type < NV_50) {
			DRM_DEBUG("global heap fallback\n");
			pramin = dev_priv->ramin_heap;
		}
	} else {
		DRM_DEBUG("global heap\n");
		pramin = dev_priv->ramin_heap;
	}

	if (!pramin) {
		DRM_ERROR("No PRAMIN heap!\n");
		return DRM_ERR(EINVAL);
	}

	if (!chan && (ret = engine->instmem.populate(dev, gpuobj, &size))) {
		nouveau_gpuobj_del(dev, &gpuobj);
		return ret;
	}

	/* Allocate a chunk of the PRAMIN aperture */
	gpuobj->im_pramin = nouveau_mem_alloc_block(pramin, size,
						    drm_order(align),
						    (DRMFILE)-2);
	if (!gpuobj->im_pramin) {
		nouveau_gpuobj_del(dev, &gpuobj);
		return DRM_ERR(ENOMEM);
	}
	gpuobj->im_pramin->flags = NOUVEAU_MEM_INSTANCE;

	if (!chan && (ret = engine->instmem.bind(dev, gpuobj))) {
		nouveau_gpuobj_del(dev, &gpuobj);
		return ret;
	}

	if (gpuobj->flags & NVOBJ_FLAG_ZERO_ALLOC) {
		int i;

		for (i = 0; i < gpuobj->im_pramin->size; i += 4)
			INSTANCE_WR(gpuobj, i/4, 0);
	}

	if (dev_priv->gpuobj_all) {
		gpuobj->next = dev_priv->gpuobj_all;
		gpuobj->next->prev = gpuobj;
	}
	dev_priv->gpuobj_all = gpuobj;

	*gpuobj_ret = gpuobj;
	return 0;
}

void nouveau_gpuobj_takedown(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_gpuobj_t *gpuobj = NULL;

	DRM_DEBUG("\n");

	while ((gpuobj = dev_priv->gpuobj_all)) {
		DRM_ERROR("gpuobj %p still exists at takedown, refs=%d\n",
			  gpuobj, gpuobj->refcount);
		gpuobj->refcount = 0;
		nouveau_gpuobj_del(dev, &gpuobj);
	}
}

int nouveau_gpuobj_del(drm_device_t *dev, nouveau_gpuobj_t **pgpuobj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_engine_func_t *engine = &dev_priv->Engine;
	nouveau_gpuobj_t *gpuobj;

	DRM_DEBUG("gpuobj %p\n", pgpuobj ? *pgpuobj : NULL);

	if (!dev_priv || !pgpuobj || !(*pgpuobj))
		return DRM_ERR(EINVAL);
	gpuobj = *pgpuobj;

	if (gpuobj->refcount != 0) {
		DRM_ERROR("gpuobj refcount is %d\n", gpuobj->refcount);
		return DRM_ERR(EINVAL);
	}

	engine->instmem.clear(dev, gpuobj);

	if (gpuobj->im_pramin) {
		if (gpuobj->flags & NVOBJ_FLAG_FAKE)
			drm_free(gpuobj->im_pramin, sizeof(*gpuobj->im_pramin),
				 DRM_MEM_DRIVER);
		else
			nouveau_mem_free_block(gpuobj->im_pramin);
	}

	if (gpuobj->next)
		gpuobj->next->prev = gpuobj->prev;
	if (gpuobj->prev)
		gpuobj->prev->next = gpuobj->next;
	else
		dev_priv->gpuobj_all = gpuobj->next;

	*pgpuobj = NULL;
	drm_free(gpuobj, sizeof(*gpuobj), DRM_MEM_DRIVER);
	return 0;
}

static int
nouveau_gpuobj_instance_get(drm_device_t *dev, int channel,
			    nouveau_gpuobj_t *gpuobj, uint32_t *inst)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_gpuobj_t *cpramin;

	/* <NV50 use PRAMIN address everywhere */
	if (dev_priv->card_type < NV_50) {
		*inst = gpuobj->im_pramin->start;
		return 0;
	}

	if ((channel > 0) && gpuobj->im_channel != channel) {
		DRM_ERROR("Channel mismatch: obj %d, ref %d\n",
			  gpuobj->im_channel, channel);
		return DRM_ERR(EINVAL);
	}

	/* NV50 channel-local instance */
	if (channel > 0) {
		cpramin = dev_priv->fifos[channel]->ramin->gpuobj;
		*inst = gpuobj->im_pramin->start - cpramin->im_pramin->start;
		return 0;
	}

	/* NV50 global (VRAM) instance */
	if (gpuobj->im_channel < 0) {
		/* ...from global heap */
		if (!gpuobj->im_backing) {
			DRM_ERROR("AII, no VRAM backing gpuobj\n");
			return DRM_ERR(EINVAL);
		}
		*inst = gpuobj->im_backing->start - dev_priv->fb_phys;
		return 0;
	} else {
		/* ...from local heap */
		cpramin = dev_priv->fifos[gpuobj->im_channel]->ramin->gpuobj;
		*inst = (cpramin->im_backing->start - dev_priv->fb_phys) +
			(gpuobj->im_pramin->start - cpramin->im_pramin->start);
		return 0;
	}

	return DRM_ERR(EINVAL);
}

int
nouveau_gpuobj_ref_add(drm_device_t *dev, int channel, uint32_t handle,
		       nouveau_gpuobj_t *gpuobj, nouveau_gpuobj_ref_t **ref_ret)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = NULL;
	nouveau_gpuobj_ref_t *ref;
	uint32_t instance;
	int ret;

	DRM_DEBUG("ch%d h=0x%08x gpuobj=%p\n", channel, handle, gpuobj);

	if (!dev_priv || !gpuobj || (ref_ret && *ref_ret != NULL))
		return DRM_ERR(EINVAL);

	if (channel >= 0) {
		if (channel > nouveau_fifo_number(dev))
			return DRM_ERR(EINVAL);
		chan = dev_priv->fifos[channel];
	} else
	if (!ref_ret)
		return DRM_ERR(EINVAL);

	ret = nouveau_gpuobj_instance_get(dev, channel, gpuobj, &instance);
	if (ret)
		return ret;

	ref = drm_calloc(1, sizeof(*ref), DRM_MEM_DRIVER);
	if (!ref)
		return DRM_ERR(ENOMEM);
	ref->gpuobj   = gpuobj;
	ref->channel  = channel;
	ref->instance = instance;

	if (!ref_ret) {
		ref->handle = handle;

		ret = nouveau_ramht_insert(dev, ref);
		if (ret) {
			drm_free(ref, sizeof(*ref), DRM_MEM_DRIVER);
			return ret;
		}

		ref->next = chan->ramht_refs;
		chan->ramht_refs = ref;
	} else {
		ref->handle = ~0;
		*ref_ret = ref;
	}

	ref->gpuobj->refcount++;
	return 0;
}

int nouveau_gpuobj_ref_del(drm_device_t *dev, nouveau_gpuobj_ref_t **pref)
{
	nouveau_gpuobj_ref_t *ref;

	DRM_DEBUG("ref %p\n", pref ? *pref : NULL);

	if (!dev || !pref || *pref == NULL)
		return DRM_ERR(EINVAL);
	ref = *pref;

	if (ref->handle != ~0)
		nouveau_ramht_remove(dev, ref);

	if (ref->gpuobj) {
		ref->gpuobj->refcount--;

		if (ref->gpuobj->refcount == 0) {
			if (!(ref->gpuobj->flags & NVOBJ_FLAG_ALLOW_NO_REFS))
				nouveau_gpuobj_del(dev, &ref->gpuobj);
		}
	}

	*pref = NULL;
	drm_free(ref, sizeof(ref), DRM_MEM_DRIVER);
	return 0;
}

int
nouveau_gpuobj_new_ref(drm_device_t *dev, int oc, int rc, uint32_t handle,
		       int size, int align, uint32_t flags,
		       nouveau_gpuobj_ref_t **ref)
{
	nouveau_gpuobj_t *gpuobj = NULL;
	int ret;

	if ((ret = nouveau_gpuobj_new(dev, oc, size, align, flags, &gpuobj)))
		return ret;

	if ((ret = nouveau_gpuobj_ref_add(dev, rc, handle, gpuobj, ref))) {
		nouveau_gpuobj_del(dev, &gpuobj);
		return ret;
	}

	return 0;
}

static int
nouveau_gpuobj_ref_find(drm_device_t *dev, int channel, uint32_t handle,
			nouveau_gpuobj_ref_t **ref_ret)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	nouveau_gpuobj_ref_t *ref = chan->ramht_refs;

	while (ref) {
		if (ref->handle == handle) {
			if (ref_ret)
				*ref_ret = ref;
			return 0;
		}
		ref = ref->next;
	}

	return DRM_ERR(EINVAL);
}

int
nouveau_gpuobj_new_fake(drm_device_t *dev, uint32_t offset, uint32_t size,
			uint32_t flags, nouveau_gpuobj_t **pgpuobj,
			nouveau_gpuobj_ref_t **pref)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_gpuobj_t *gpuobj = NULL;
	int i;

	DRM_DEBUG("offset=0x%08x size=0x%08x flags=0x%08x\n",
		  offset, size, flags);

	gpuobj = drm_calloc(1, sizeof(*gpuobj), DRM_MEM_DRIVER);
	if (!gpuobj)
		return DRM_ERR(ENOMEM);
	DRM_DEBUG("gpuobj %p\n", gpuobj);
	gpuobj->im_channel = -1;
	gpuobj->flags      = flags | NVOBJ_FLAG_FAKE;

	gpuobj->im_pramin = drm_calloc(1, sizeof(struct mem_block),
				       DRM_MEM_DRIVER);
	if (!gpuobj->im_pramin) {
		nouveau_gpuobj_del(dev, &gpuobj);
		return DRM_ERR(ENOMEM);
	}
	gpuobj->im_pramin->start = offset;
	gpuobj->im_pramin->size  = size;

	if (gpuobj->flags & NVOBJ_FLAG_ZERO_ALLOC) {
		for (i = 0; i < gpuobj->im_pramin->size; i += 4)
			INSTANCE_WR(gpuobj, i/4, 0);
	}

	if (pref) {
		if ((i = nouveau_gpuobj_ref_add(dev, -1, 0, gpuobj, pref))) {
			nouveau_gpuobj_del(dev, &gpuobj);
			return i;
		}
	}

	if (pgpuobj)
		*pgpuobj = gpuobj;
	return 0;
}


static int
nouveau_gpuobj_class_instmem_size(drm_device_t *dev, int class)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/*XXX: dodgy hack for now */
	if (dev_priv->card_type >= NV_50)
		return 24;
	if (dev_priv->card_type >= NV_40)
		return 32;
	return 16;
}

/*
   DMA objects are used to reference a piece of memory in the
   framebuffer, PCI or AGP address space. Each object is 16 bytes big
   and looks as follows:
   
   entry[0]
   11:0  class (seems like I can always use 0 here)
   12    page table present?
   13    page entry linear?
   15:14 access: 0 rw, 1 ro, 2 wo
   17:16 target: 0 NV memory, 1 NV memory tiled, 2 PCI, 3 AGP
   31:20 dma adjust (bits 0-11 of the address)
   entry[1]
   dma limit (size of transfer)
   entry[X]
   1     0 readonly, 1 readwrite
   31:12 dma frame address of the page (bits 12-31 of the address)
   entry[N]
   page table terminator, same value as the first pte, as does nvidia
   rivatv uses 0xffffffff

   Non linear page tables need a list of frame addresses afterwards,
   the rivatv project has some info on this.

   The method below creates a DMA object in instance RAM and returns a handle
   to it that can be used to set up context objects.
*/
int
nouveau_gpuobj_dma_new(drm_device_t *dev, int channel, int class,
		       uint64_t offset, uint64_t size, int access, int target,
		       nouveau_gpuobj_t **gpuobj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int ret;
	uint32_t is_scatter_gather = 0;
	
	DRM_DEBUG("ch%d class=0x%04x offset=0x%llx size=0x%llx\n",
		  channel, class, offset, size);
	DRM_DEBUG("access=%d target=%d\n", access, target);

	switch (target) {
        case NV_DMA_TARGET_AGP:
                 offset += dev_priv->agp_phys;
                 break;
        case NV_DMA_TARGET_PCI_NONLINEAR:
                /*assume the "offset" is a virtual memory address*/
                is_scatter_gather = 1;
                /*put back the right value*/
                target = NV_DMA_TARGET_PCI;
                break;
        default:
                break;
        }
	
	ret = nouveau_gpuobj_new(dev, channel,
				 is_scatter_gather ? ((((size + PAGE_SIZE - 1) / PAGE_SIZE) << 2) + 12) : nouveau_gpuobj_class_instmem_size(dev, class),
				 16,
				 NVOBJ_FLAG_ZERO_ALLOC | NVOBJ_FLAG_ZERO_FREE,
				 gpuobj);
	if (ret) {
		DRM_ERROR("Error creating gpuobj: %d\n", ret);
		return ret;
	}

	if (dev_priv->card_type < NV_50) {
		uint32_t frame, adjust, pte_flags = 0;
		adjust = offset &  0x00000fff;
		if (access != NV_DMA_ACCESS_RO)
				pte_flags |= (1<<1);
		
		if ( ! is_scatter_gather ) 
			{
			frame  = offset & ~0x00000fff;
			
			INSTANCE_WR(*gpuobj, 0, ((1<<12) | (1<<13) |
					(adjust << 20) |
					 (access << 14) |
					 (target << 16) |
					  class));
			INSTANCE_WR(*gpuobj, 1, size - 1);
			INSTANCE_WR(*gpuobj, 2, frame | pte_flags);
			INSTANCE_WR(*gpuobj, 3, frame | pte_flags);
			}
		else 
			{
			uint32_t instance_offset;
			uint64_t bus_addr;
			size = (uint32_t) size;

			DRM_DEBUG("Creating PCI DMA object using virtual zone starting at %#llx, size %d\n", offset, (uint32_t)size);
	                INSTANCE_WR(*gpuobj, 0, ((1<<12) | (0<<13) |
                                (adjust << 20) |
                                (access << 14) |
                                (target << 16) |
                                class));
			INSTANCE_WR(*gpuobj, 1, size-1);

			/*write starting at the third dword*/
			instance_offset = 2;
 
			/*for each PAGE, get its bus address, fill in the page table entry, and advance*/
			while ( size > 0 ) {
				bus_addr = vmalloc_to_page(offset);
 				if ( ! bus_addr ) 
					{
					DRM_ERROR("Couldn't map virtual address %#llx to a page number\n", offset);
					nouveau_gpuobj_del(dev, gpuobj);
					return DRM_ERR(ENOMEM);
					}
				bus_addr = (uint64_t) page_address(bus_addr);
 				if ( ! bus_addr ) 
					{
					DRM_ERROR("Couldn't find page address for address %#llx\n", offset);
					nouveau_gpuobj_del(dev, gpuobj);
					return DRM_ERR(ENOMEM);
					}
				bus_addr |= (offset & ~PAGE_MASK);
				bus_addr = virt_to_bus((void *)bus_addr);
 				if ( ! bus_addr ) 
					{
					DRM_ERROR("Couldn't get bus address for %#llx\n", offset);
					nouveau_gpuobj_del(dev, gpuobj);
					return DRM_ERR(ENOMEM);
					}

				/*if ( bus_addr >= 1 << 32 )
					{
					DRM_ERROR("Bus address %#llx is over 32 bits, Nvidia cards cannot address it !\n", bus_addr);
					nouveau_gpuobj_del(dev, gpuobj);
					return DRM_ERR(EINVAL);
					}*/
				
				frame = (uint32_t) bus_addr & ~0x00000FFF;
				INSTANCE_WR(*gpuobj, instance_offset, frame | pte_flags);
				offset += PAGE_SIZE;
				instance_offset ++;
				size -= PAGE_SIZE;
				}

			}
	} else {
		INSTANCE_WR(*gpuobj, 0, 0x00190000 | class);
		INSTANCE_WR(*gpuobj, 1, offset + size - 1);
		INSTANCE_WR(*gpuobj, 2, offset);
		INSTANCE_WR(*gpuobj, 5, 0x00010000);
	}

	(*gpuobj)->engine = NVOBJ_ENGINE_SW;
	(*gpuobj)->class  = class;
	return 0;
}

/* Context objects in the instance RAM have the following structure.
 * On NV40 they are 32 byte long, on NV30 and smaller 16 bytes.

   NV4 - NV30:

   entry[0]
   11:0 class
   12   chroma key enable
   13   user clip enable
   14   swizzle enable
   17:15 patch config:
       scrcopy_and, rop_and, blend_and, scrcopy, srccopy_pre, blend_pre
   18   synchronize enable
   19   endian: 1 big, 0 little
   21:20 dither mode
   23    single step enable
   24    patch status: 0 invalid, 1 valid
   25    context_surface 0: 1 valid
   26    context surface 1: 1 valid
   27    context pattern: 1 valid
   28    context rop: 1 valid
   29,30 context beta, beta4
   entry[1]
   7:0   mono format
   15:8  color format
   31:16 notify instance address
   entry[2]
   15:0  dma 0 instance address
   31:16 dma 1 instance address
   entry[3]
   dma method traps

   NV40:
   No idea what the exact format is. Here's what can be deducted:

   entry[0]:
   11:0  class  (maybe uses more bits here?)
   17    user clip enable
   21:19 patch config 
   25    patch status valid ?
   entry[1]:
   15:0  DMA notifier  (maybe 20:0)
   entry[2]:
   15:0  DMA 0 instance (maybe 20:0)
   24    big endian
   entry[3]:
   15:0  DMA 1 instance (maybe 20:0)
   entry[4]:
   entry[5]:
   set to 0?
*/
int
nouveau_gpuobj_gr_new(drm_device_t *dev, int channel, int class,
		      nouveau_gpuobj_t **gpuobj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int ret;

	DRM_DEBUG("ch%d class=0x%04x\n", channel, class);

	ret = nouveau_gpuobj_new(dev, channel,
				 nouveau_gpuobj_class_instmem_size(dev, class),
				 16,
				 NVOBJ_FLAG_ZERO_ALLOC | NVOBJ_FLAG_ZERO_FREE,
				 gpuobj);
	if (ret) {
		DRM_ERROR("Error creating gpuobj: %d\n", ret);
		return ret;
	}

	if (dev_priv->card_type >= NV_50) {
		INSTANCE_WR(*gpuobj, 0, class);
		INSTANCE_WR(*gpuobj, 5, 0x00010000);
	} else {
	switch (class) {
	case NV_CLASS_NULL:
		INSTANCE_WR(*gpuobj, 0, 0x00001030);
		INSTANCE_WR(*gpuobj, 1, 0xFFFFFFFF);
		break;
	default:
		if (dev_priv->card_type >= NV_40) {
			INSTANCE_WR(*gpuobj, 0, class);
#ifdef __BIG_ENDIAN
			INSTANCE_WR(*gpuobj, 2, 0x01000000);
#endif
		} else {
#ifdef __BIG_ENDIAN
			INSTANCE_WR(*gpuobj, 0, class | 0x00080000);
#else
			INSTANCE_WR(*gpuobj, 0, class);
#endif
		}
	}
	}

	(*gpuobj)->engine = NVOBJ_ENGINE_GR;
	(*gpuobj)->class  = class;
	return 0;
}

static int
nouveau_gpuobj_channel_init_pramin(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	nouveau_gpuobj_t *pramin = NULL;
	int size, base, ret;

	DRM_DEBUG("ch%d\n", channel);

	/* Base amount for object storage (4KiB enough?) */
	size = 0x1000;
	base = 0;

	/* PGRAPH context */

	if (dev_priv->card_type == NV_50) {
		/* Various fixed table thingos */
		size += 0x1400; /* mostly unknown stuff */
		size += 0x4000; /* vm pd */
		base  = 0x6000;
		/* RAMHT, not sure about setting size yet, 32KiB to be safe */
		size += 0x8000;
		/* RAMFC */
		size += 0x1000;
		/* PGRAPH context */
		size += 0x60000;
	}

	DRM_DEBUG("ch%d PRAMIN size: 0x%08x bytes, base alloc=0x%08x\n",
		  channel, size, base);
	ret = nouveau_gpuobj_new_ref(dev, -1, -1, 0, size, 0x1000, 0,
				     &chan->ramin);
	if (ret) {
		DRM_ERROR("Error allocating channel PRAMIN: %d\n", ret);
		return ret;
	}
	pramin = chan->ramin->gpuobj;

	ret = nouveau_mem_init_heap(&chan->ramin_heap,
				    pramin->im_pramin->start + base, size);
	if (ret) {
		DRM_ERROR("Error creating PRAMIN heap: %d\n", ret);
		nouveau_gpuobj_ref_del(dev, &chan->ramin);
		return ret;
	}

	return 0;
}

int
nouveau_gpuobj_channel_init(drm_device_t *dev, int channel,
			    uint32_t vram_h, uint32_t tt_h)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	nouveau_gpuobj_t *vram = NULL, *tt = NULL;
	int ret;

	DRM_DEBUG("ch%d vram=0x%08x tt=0x%08x\n", channel, vram_h, tt_h);

	/* Reserve a block of PRAMIN for the channel
	 *XXX: maybe on <NV50 too at some point
	 */
	if (0 || dev_priv->card_type == NV_50) {
		ret = nouveau_gpuobj_channel_init_pramin(dev, channel);
		if (ret)
			return ret;
	}

	/* RAMHT */
	if (dev_priv->card_type < NV_50) {
		ret = nouveau_gpuobj_ref_add(dev, -1, 0, dev_priv->ramht,
					     &chan->ramht);
		if (ret)
			return ret;
	} else {
		ret = nouveau_gpuobj_new_ref(dev, channel, channel, 0,
					     0x8000, 16,
					     NVOBJ_FLAG_ZERO_ALLOC,
					     &chan->ramht);
		if (ret)
			return ret;
	}

	/* VRAM ctxdma */
	if ((ret = nouveau_gpuobj_dma_new(dev, channel, NV_CLASS_DMA_IN_MEMORY,
					  0, dev_priv->fb_available_size,
					  NV_DMA_ACCESS_RW,
					  NV_DMA_TARGET_VIDMEM, &vram))) {
		DRM_ERROR("Error creating VRAM ctxdma: %d\n", ret);
		return ret;
	}

	if ((ret = nouveau_gpuobj_ref_add(dev, channel, vram_h, vram, NULL))) {
		DRM_ERROR("Error referencing VRAM ctxdma: %d\n", ret);
		return ret;
	}

	if (dev_priv->agp_heap) {
		/* AGPGART ctxdma */
		if ((ret = nouveau_gpuobj_dma_new(dev, channel, NV_CLASS_DMA_IN_MEMORY,
						   0, dev_priv->agp_available_size,
						   NV_DMA_ACCESS_RW,
						   NV_DMA_TARGET_AGP, &tt))) {
			DRM_ERROR("Error creating AGP TT ctxdma: %d\n", DRM_ERR(ENOMEM));
			return DRM_ERR(ENOMEM);
		}
	
		ret = nouveau_gpuobj_ref_add(dev, channel, tt_h, tt, NULL);
		if (ret) {
			DRM_ERROR("Error referencing AGP TT ctxdma: %d\n", ret);
			return ret;
		}
	}
	else {
		if ( dev_priv -> card_type >= NV_50 ) return 0; /*no PCIGART for NV50*/

		/*PCI*/
		if((ret = nouveau_gpuobj_dma_new(dev, channel, NV_CLASS_DMA_IN_MEMORY,
						   dev->sg->virtual, dev->sg->pages * PAGE_SIZE,
						   NV_DMA_ACCESS_RW,
						   NV_DMA_TARGET_PCI_NONLINEAR, &tt))) {
			DRM_ERROR("Error creating PCI TT ctxdma: %d\n", DRM_ERR(ENOMEM));
			return 0; //this is noncritical
		}
	
		ret = nouveau_gpuobj_ref_add(dev, channel, tt_h, tt, NULL);
		if (ret) {
			DRM_ERROR("Error referencing PCI TT ctxdma: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

void
nouveau_gpuobj_channel_takedown(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	nouveau_gpuobj_ref_t *ref;

	DRM_DEBUG("ch%d\n", channel);

	while ((ref = chan->ramht_refs)) {
		chan->ramht_refs = ref->next;
		nouveau_gpuobj_ref_del(dev, &ref);
	}
	nouveau_gpuobj_ref_del(dev, &chan->ramht);

	if (chan->ramin_heap)
		nouveau_mem_takedown(&chan->ramin_heap);
	if (chan->ramin)
		nouveau_gpuobj_ref_del(dev, &chan->ramin);

}

int nouveau_ioctl_grobj_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_grobj_alloc_t init;
	nouveau_gpuobj_t *gr = NULL;
	int ret;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_grobj_alloc_t __user *)
		data, sizeof(init));

	if (!nouveau_fifo_owner(dev, filp, init.channel)) {
		DRM_ERROR("pid %d doesn't own channel %d\n",
				DRM_CURRENTPID, init.channel);
		return DRM_ERR(EINVAL);
	}

	//FIXME: check args, only allow trusted objects to be created
	
	if (init.handle == ~0)
		return DRM_ERR(EINVAL);
	if (nouveau_gpuobj_ref_find(dev, init.channel, init.handle, NULL) == 0)
		return DRM_ERR(EEXIST);

	if ((ret = nouveau_gpuobj_gr_new(dev, init.channel, init.class, &gr))) {
		DRM_ERROR("Error creating gr object: %d (%d/0x%08x)\n",
			  ret, init.channel, init.handle);
		return ret;
	}

	if ((ret = nouveau_gpuobj_ref_add(dev, init.channel, init.handle,
					  gr, NULL))) {
		DRM_ERROR("Error referencing gr object: %d (%d/0x%08x\n)",
			  ret, init.channel, init.handle);
		nouveau_gpuobj_del(dev, &gr);
		return ret;
	}

	return 0;
}


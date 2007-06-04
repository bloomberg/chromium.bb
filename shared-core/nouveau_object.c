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

/* TODO
 *  - Check object class, deny unsafe objects (add card-specific versioning?)
 *  - Get rid of DMA object creation, this should be wrapped by MM routines.
 */

/* Translate a RAMIN offset into a value the card understands, will be useful
 * in the future when we can access more instance ram which isn't mapped into
 * the PRAMIN aperture
 */
uint32_t
nouveau_chip_instance_get(drm_device_t *dev, struct mem_block *mem)
{
	uint32_t inst = (uint32_t)mem->start >> 4;
	DRM_DEBUG("****** on-chip instance for 0x%016llx = 0x%08x\n",
			mem->start, inst);
	return inst;
}

static void
nouveau_object_link(drm_device_t *dev, struct nouveau_object *obj)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[obj->channel];

	if (!chan->objs) {
		chan->objs = obj;
		return;
	}

	obj->prev = NULL;
	obj->next = chan->objs;

	chan->objs->prev = obj;
	chan->objs = obj;
}

static void
nouveau_object_unlink(drm_device_t *dev, struct nouveau_object *obj)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[obj->channel];

	if (obj->prev == NULL) {	
		if (obj->next)
			obj->next->prev = NULL;
		chan->objs = obj->next;
	} else if (obj->next == NULL) {
		if (obj->prev)
			obj->prev->next = NULL;
	} else {
		obj->prev->next = obj->next;
		obj->next->prev = obj->prev;
	}
}

static struct nouveau_object *
nouveau_object_handle_find(drm_device_t *dev, int channel, uint32_t handle)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	struct nouveau_object *obj = chan->objs;

	DRM_DEBUG("Looking for handle 0x%08x\n", handle);
	while (obj) {
		if (obj->handle == handle)
			return obj;
		obj = obj->next;
	}

	DRM_DEBUG("...couldn't find handle\n");
	return NULL;
}

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
nouveau_ht_handle_hash(drm_device_t *dev, int channel, uint32_t handle)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	uint32_t hash = 0;
	int i;

	for (i=32;i>0;i-=dev_priv->ramht_bits) {
		hash ^= (handle & ((1 << dev_priv->ramht_bits) - 1));
		handle >>= dev_priv->ramht_bits;
	}
	hash ^= channel << (dev_priv->ramht_bits - 4);
	return hash << 3;
}

static int
nouveau_ht_object_insert(drm_device_t* dev, int channel, uint32_t handle,
			 struct nouveau_object *obj)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	int ht_base = NV_RAMIN + dev_priv->ramht_offset;
	int ht_end  = ht_base + dev_priv->ramht_size;
	int o_ofs, ofs;

	obj->handle = handle;
	o_ofs = ofs = nouveau_ht_handle_hash(dev, channel, obj->handle);

	while (NV_READ(ht_base + ofs) || NV_READ(ht_base + ofs + 4)) {
		ofs += 8;
		if (ofs == dev_priv->ramht_size) ofs = 0;
		if (ofs == o_ofs) {
			DRM_ERROR("no free hash table entries\n");
			return 1;
		}
	}
	ofs += ht_base;

	DRM_DEBUG("Channel %d - Handle 0x%08x at 0x%08x\n",
		channel, obj->handle, ofs);

	NV_WRITE(NV_RAMHT_HANDLE_OFFSET + ofs, obj->handle);
	if (dev_priv->card_type >= NV_40)
		NV_WRITE(NV_RAMHT_CONTEXT_OFFSET + ofs,
			(channel     << NV40_RAMHT_CONTEXT_CHANNEL_SHIFT) |
			(obj->engine << NV40_RAMHT_CONTEXT_ENGINE_SHIFT) |
			nouveau_chip_instance_get(dev, obj->instance)
			);	    
	else
		NV_WRITE(NV_RAMHT_CONTEXT_OFFSET + ofs,
			NV_RAMHT_CONTEXT_VALID |
			(channel     << NV_RAMHT_CONTEXT_CHANNEL_SHIFT) |
			(obj->engine << NV_RAMHT_CONTEXT_ENGINE_SHIFT) |
			nouveau_chip_instance_get(dev, obj->instance)
			);

	obj->ht_loc = ofs;
	return 0;
}

static void nouveau_hash_table_remove(drm_device_t* dev,
				      struct nouveau_object *obj)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("Remove handle 0x%08x at 0x%08x from HT\n",
			obj->handle, obj->ht_loc);
	if (obj->ht_loc) {
		DRM_DEBUG("... HT entry was: 0x%08x/0x%08x\n",
				NV_READ(obj->ht_loc), NV_READ(obj->ht_loc+4));
		NV_WRITE(obj->ht_loc  , 0x00000000);
		NV_WRITE(obj->ht_loc+4, 0x00000000);
	}
}

static struct nouveau_object *
nouveau_object_instance_alloc(drm_device_t* dev, int channel)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct nouveau_object       *obj;

	/* Create object struct */
	obj = drm_calloc(1, sizeof(struct nouveau_object), DRM_MEM_DRIVER);
	if (!obj) {
		DRM_ERROR("couldn't alloc memory for object\n");
		return NULL;
	}

	/* Allocate instance memory */
	obj->instance  = nouveau_instmem_alloc(dev,
			(dev_priv->card_type >= NV_40 ? 32 : 16), 4);
	if (!obj->instance) {
		DRM_ERROR("couldn't alloc RAMIN for object\n");
		drm_free(obj, sizeof(struct nouveau_object), DRM_MEM_DRIVER);
		return NULL;
	}

	/* Bind object to channel */
	obj->channel = channel;
	obj->handle  = ~0;
	nouveau_object_link(dev, obj);

	return obj;
}

static void
nouveau_object_instance_free(drm_device_t *dev, struct nouveau_object *obj)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	int i;

	/* Unbind object from channel */
	nouveau_object_unlink(dev, obj);

	/* Clean RAMIN entry */
	DRM_DEBUG("Instance entry for 0x%08x"
		"(engine %d, class 0x%x) before destroy:\n",
		obj->handle, obj->engine, obj->class);
	for (i=0; i<(obj->instance->size/4); i++) {
		DRM_DEBUG("  +0x%02x: 0x%08x\n", (i*4),
			INSTANCE_RD(obj->instance, i));
		INSTANCE_WR(obj->instance, i, 0x00000000);
	}

	/* Free RAMIN */
	nouveau_instmem_free(dev, obj->instance);
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
   dma limit
   entry[2]
   1     0 readonly, 1 readwrite
   31:12 dma frame address (bits 12-31 of the address)

   Non linear page tables seem to need a list of frame addresses afterwards,
   the rivatv project has some info on this.   

   The method below creates a DMA object in instance RAM and returns a handle
   to it that can be used to set up context objects.
*/

struct nouveau_object *
nouveau_object_dma_create(drm_device_t* dev, int channel, int class,
			  uint32_t offset, uint32_t size,
			  int access, int target)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct   nouveau_object *obj;
	uint32_t frame, adjust;
	uint32_t pte_flags = 0;

	DRM_DEBUG("offset:0x%08x, size:0x%08x, target:%d, access:%d\n",
			offset, size, target, access);

	switch (target) {
	case NV_DMA_TARGET_AGP:
		offset += dev_priv->agp_phys;
		break;
	default:
		break;
	}

	switch (access) {
	case NV_DMA_ACCESS_RO:
		break;
	case NV_DMA_ACCESS_WO:
	case NV_DMA_ACCESS_RW:
		pte_flags  |= (1 << 1);
		break;
	default:
		DRM_ERROR("invalid access mode=%d\n", access);
		return NULL;
	}

	frame  = offset & ~0x00000FFF;
	adjust = offset &  0x00000FFF;

	obj = nouveau_object_instance_alloc(dev, channel);
	if (!obj) {
		DRM_ERROR("couldn't allocate DMA object\n");
		return obj;
	}

	obj->engine = 0;
	obj->class  = class;

	INSTANCE_WR(obj->instance, 0, ((1<<12) | (1<<13) |
				(adjust << 20) |
				(access << 14) |
				(target << 16) |
				class));
	INSTANCE_WR(obj->instance, 1, size-1);
	INSTANCE_WR(obj->instance, 2, frame | pte_flags);
	INSTANCE_WR(obj->instance, 3, frame | pte_flags);

	return obj;
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
struct nouveau_object *
nouveau_object_gr_create(drm_device_t* dev, int channel, int class)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	struct   nouveau_object *obj;

	DRM_DEBUG("class=%x\n",	class);

	obj = nouveau_object_instance_alloc(dev, channel);
	if (!obj) {
		DRM_ERROR("couldn't allocate context object\n");
		return obj;
	}

	obj->engine = 1;
	obj->class  = class;

	switch (class) {
	case NV_CLASS_NULL:
		INSTANCE_WR(obj->instance, 0, 0x00001030);
		INSTANCE_WR(obj->instance, 1, 0xFFFFFFFF);
		INSTANCE_WR(obj->instance, 2, 0x00000000);
		INSTANCE_WR(obj->instance, 2, 0x00000000);
		break;
	default:
		if (dev_priv->card_type >= NV_40) {
			INSTANCE_WR(obj->instance, 0,  obj->class);
			INSTANCE_WR(obj->instance, 1, 0x00000000);
#ifdef __BIG_ENDIAN
			INSTANCE_WR(obj->instance, 2, 0x01000000);
#else
			INSTANCE_WR(obj->instance, 2, 0x00000000);
#endif
			INSTANCE_WR(obj->instance, 3, 0x00000000);
			INSTANCE_WR(obj->instance, 4, 0x00000000);
			INSTANCE_WR(obj->instance, 5, 0x00000000);
			INSTANCE_WR(obj->instance, 6, 0x00000000);
			INSTANCE_WR(obj->instance, 7, 0x00000000);
		} else {
#ifdef __BIG_ENDIAN
			INSTANCE_WR(obj->instance, 0, obj->class | 0x00080000);
#else
			INSTANCE_WR(obj->instance, 0, obj->class);
#endif
			INSTANCE_WR(obj->instance, 1, 0x00000000);
			INSTANCE_WR(obj->instance, 2, 0x00000000);
			INSTANCE_WR(obj->instance, 3, 0x00000000);
		}
	}

	return obj;
}

void
nouveau_object_free(drm_device_t *dev, struct nouveau_object *obj)
{
	nouveau_object_instance_free(dev, obj);
	if (obj->handle != ~0)
		nouveau_hash_table_remove(dev, obj);
	drm_free(obj, sizeof(struct nouveau_object), DRM_MEM_DRIVER);
}

void nouveau_object_cleanup(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;

	while (dev_priv->fifos[channel].objs) {
		nouveau_object_free(dev, dev_priv->fifos[channel].objs);
	}
}

int nouveau_ioctl_object_init(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_object_init_t init;
	struct nouveau_object *obj;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_object_init_t __user *)
		data, sizeof(init));

	if (!nouveau_fifo_owner(dev, filp, init.channel)) {
		DRM_ERROR("pid %d doesn't own channel %d\n",
				DRM_CURRENTPID, init.channel);
		return DRM_ERR(EINVAL);
	}

	//FIXME: check args, only allow trusted objects to be created

	if (nouveau_object_handle_find(dev, init.channel, init.handle)) {
		DRM_ERROR("Channel %d: handle 0x%08x already exists\n",
			init.channel, init.handle);
		return DRM_ERR(EINVAL);
	}

	obj = nouveau_object_gr_create(dev, init.channel, init.class);
	if (!obj)
		return DRM_ERR(ENOMEM);

	if (nouveau_ht_object_insert(dev, init.channel, init.handle, obj)) {
		nouveau_object_free(dev, obj);
		return DRM_ERR(ENOMEM);
	}

	return 0;
}

static int
nouveau_dma_object_check_access(drm_device_t *dev,
				drm_nouveau_dma_object_init_t *init)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint64_t limit;

	/* Check for known DMA object classes */
	switch (init->class) {
	case NV_CLASS_DMA_IN_MEMORY:
	case NV_CLASS_DMA_FROM_MEMORY:
	case NV_CLASS_DMA_TO_MEMORY:
		break;
	default:
		DRM_ERROR("invalid class = 0x%x\n", init->class);
		return DRM_ERR(EPERM);
	}

	/* Check access mode, and translate to NV_DMA_ACCESS_* */
	switch (init->access) {
	case NOUVEAU_MEM_ACCESS_RO:
		init->access = NV_DMA_ACCESS_RO;
		break;
	case NOUVEAU_MEM_ACCESS_WO:
		init->access = NV_DMA_ACCESS_WO;
		break;
	case NOUVEAU_MEM_ACCESS_RW:
		init->access = NV_DMA_ACCESS_RW;
		break;
	default:
		DRM_ERROR("invalid access mode = %d\n", init->access);
		return DRM_ERR(EPERM);
	}

	/* Check that request is within the allowed limits of "target" */
	switch (init->target) {
	case NOUVEAU_MEM_FB:
		limit = dev_priv->fb_available_size;
		init->target = NV_DMA_TARGET_VIDMEM;
		break;
	case NOUVEAU_MEM_AGP:
		limit = dev_priv->agp_available_size;
		init->target = NV_DMA_TARGET_AGP;
		break;
	default:
		DRM_ERROR("invalid target = 0x%x\n", init->target);
		return DRM_ERR(EPERM);
	}

	if ((init->offset > limit) || (init->offset + init->size) > limit) {
		DRM_ERROR("access out of allowed range (%d,0x%08x,0x%08x)\n",
				init->target, init->offset, init->size);
		return DRM_ERR(EPERM);
	}

	return 0;
}

int nouveau_ioctl_dma_object_init(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_dma_object_init_t init;
	struct nouveau_object *obj;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_dma_object_init_t __user *)
		data, sizeof(init));

	if (!nouveau_fifo_owner(dev, filp, init.channel)) {
		DRM_ERROR("pid %d doesn't own channel %d\n",
				DRM_CURRENTPID, init.channel);
		return DRM_ERR(EINVAL);
	}

	if (nouveau_dma_object_check_access(dev, &init))
		return DRM_ERR(EPERM);

	if (nouveau_object_handle_find(dev, init.channel, init.handle)) {
		DRM_ERROR("Channel %d: handle 0x%08x already exists\n",
			init.channel, init.handle);
		return DRM_ERR(EINVAL);
	}

	obj = nouveau_object_dma_create(dev, init.channel, init.class,
					     init.offset, init.size,
					     init.access, init.target);
	if (!obj)
		return DRM_ERR(ENOMEM);

	obj->handle = init.handle;
	if (nouveau_ht_object_insert(dev, init.channel, init.handle, obj)) {
		nouveau_object_free(dev, obj);
		return DRM_ERR(ENOMEM);
	}

	return 0;
}


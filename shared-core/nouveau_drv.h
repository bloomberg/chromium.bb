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

#ifndef __NOUVEAU_DRV_H__
#define __NOUVEAU_DRV_H__

#define DRIVER_AUTHOR		"Stephane Marchesin"
#define DRIVER_EMAIL		"dri-devel@lists.sourceforge.net"

#define DRIVER_NAME		"nouveau"
#define DRIVER_DESC		"nVidia Riva/TNT/GeForce"
#define DRIVER_DATE		"20060213"

#define DRIVER_MAJOR		0
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	6

#define NOUVEAU_FAMILY   0x0000FFFF
#define NOUVEAU_FLAGS    0xFFFF0000

#include "nouveau_drm.h"
#include "nouveau_reg.h"

struct mem_block {
	struct mem_block *next;
	struct mem_block *prev;
	uint64_t start;
	uint64_t size;
	DRMFILE filp;           /* 0: free, -1: heap, other: real files */
	int flags;
	drm_local_map_t *map;
};

enum nouveau_flags {
	NV_NFORCE   =0x10000000,
	NV_NFORCE2  =0x20000000
};

struct nouveau_object
{
	struct nouveau_object *next;
	struct nouveau_object *prev;
	int channel;

	struct mem_block *instance;
	uint32_t          ht_loc;

	uint32_t handle;
	int      class;
	int      engine;
};

struct nouveau_fifo
{
	int used;
	/* owner of this fifo */
	DRMFILE filp;
	/* mapping of the fifo itself */
	drm_local_map_t *map;
	/* mapping of the regs controling the fifo */
	drm_local_map_t *regs;
	/* dma object for the command buffer itself */
	struct mem_block      *cmdbuf_mem;
	struct nouveau_object *cmdbuf_obj;
	/* PGRAPH context, for cards that keep it in RAMIN */
	struct mem_block *ramin_grctx;
	/* objects belonging to this fifo */
	struct nouveau_object *objs;

	/* XXX dynamic alloc ? */
	uint32_t pgraph_ctx [340];
};

struct nouveau_config {
	struct {
		int location;
		int size;
	} cmdbuf;
};

struct nouveau_engine_func {
	struct {
		int	(*Init)(drm_device_t *dev);
		void	(*Takedown)(drm_device_t *dev);
	} Mc;

	struct {
		int	(*Init)(drm_device_t *dev);
		void	(*Takedown)(drm_device_t *dev);
	} Timer;

	struct {
		int	(*Init)(drm_device_t *dev);
		void	(*Takedown)(drm_device_t *dev);
	} Fb;

	struct {
		int	(*Init)(drm_device_t *dev);
		void	(*Takedown)(drm_device_t *dev);
	} Graph;

	struct {
		int	(*Init)(drm_device_t *dev);
		void	(*Takedown)(drm_device_t *dev);
	} Fifo;
};

typedef struct drm_nouveau_private {
	/* the card type, takes NV_* as values */
	int card_type;
	/* exact chipset, derived from NV_PMC_BOOT_0 */
	int chipset;
	int flags;

	drm_local_map_t *mmio;
	drm_local_map_t *fb;
	drm_local_map_t *ramin; /* NV40 onwards */

	int fifo_alloc_count;
	struct nouveau_fifo fifos[NV_MAX_FIFO_NUMBER];

	struct nouveau_engine_func Engine;

	/* RAMIN configuration, RAMFC, RAMHT and RAMRO offsets */
	uint32_t ramin_size;
	uint32_t ramht_offset;
	uint32_t ramht_size;
	uint32_t ramht_bits;
	uint32_t ramfc_offset;
	uint32_t ramfc_size;
	uint32_t ramro_offset;
	uint32_t ramro_size;

	/* base physical adresses */
	uint64_t fb_phys;
	uint64_t fb_available_size;
	uint64_t agp_phys;
	uint64_t agp_available_size;

	/* the mtrr covering the FB */
	int fb_mtrr;

	struct mem_block *agp_heap;
	struct mem_block *fb_heap;
	struct mem_block *fb_nomap_heap;
	struct mem_block *ramin_heap;

        /* context table pointed to be NV_PGRAPH_CHANNEL_CTX_TABLE (0x400780) */
        uint32_t ctx_table_size;
        struct mem_block *ctx_table;

	struct nouveau_config config;
}
drm_nouveau_private_t;

/* nouveau_state.c */
extern void nouveau_preclose(drm_device_t * dev, DRMFILE filp);
extern int nouveau_load(struct drm_device *dev, unsigned long flags);
extern int nouveau_firstopen(struct drm_device *dev);
extern void nouveau_lastclose(struct drm_device *dev);
extern int nouveau_unload(struct drm_device *dev);
extern int nouveau_ioctl_getparam(DRM_IOCTL_ARGS);
extern int nouveau_ioctl_setparam(DRM_IOCTL_ARGS);
extern void nouveau_wait_for_idle(struct drm_device *dev);
extern int nouveau_ioctl_card_init(DRM_IOCTL_ARGS);

/* nouveau_mem.c */
extern uint64_t          nouveau_mem_fb_amount(struct drm_device *dev);
extern void              nouveau_mem_release(DRMFILE filp, struct mem_block *heap);
extern int               nouveau_ioctl_mem_alloc(DRM_IOCTL_ARGS);
extern int               nouveau_ioctl_mem_free(DRM_IOCTL_ARGS);
extern struct mem_block* nouveau_mem_alloc(struct drm_device *dev, int alignment, uint64_t size, int flags, DRMFILE filp);
extern void              nouveau_mem_free(struct drm_device* dev, struct mem_block*);
extern int               nouveau_mem_init(struct drm_device *dev);
extern void              nouveau_mem_close(struct drm_device *dev);
extern int               nouveau_instmem_init(struct drm_device *dev);
extern struct mem_block* nouveau_instmem_alloc(struct drm_device *dev,
					       uint32_t size, uint32_t align);
extern void              nouveau_instmem_free(struct drm_device *dev,
					      struct mem_block *block);
extern uint32_t          nouveau_instmem_r32(drm_nouveau_private_t *dev_priv,
					     struct mem_block *mem, int index);
extern void              nouveau_instmem_w32(drm_nouveau_private_t *dev_priv,
					     struct mem_block *mem, int index,
					     uint32_t val);

/* nouveau_fifo.c */
extern int  nouveau_fifo_init(drm_device_t *dev);
extern int  nouveau_fifo_number(drm_device_t *dev);
extern int  nouveau_fifo_ctx_size(drm_device_t *dev);
extern void nouveau_fifo_cleanup(drm_device_t *dev, DRMFILE filp);
extern int  nouveau_fifo_owner(drm_device_t *dev, DRMFILE filp, int channel);
extern void nouveau_fifo_free(drm_device_t *dev, int channel);

/* nouveau_object.c */
extern void nouveau_object_cleanup(drm_device_t *dev, int channel);
extern struct nouveau_object *
nouveau_object_gr_create(drm_device_t *dev, int channel, int class);
extern struct nouveau_object *
nouveau_object_dma_create(drm_device_t *dev, int channel, int class,
			  uint32_t offset, uint32_t size,
			  int access, int target);
extern void nouveau_object_free(drm_device_t *dev, struct nouveau_object *obj);
extern int  nouveau_ioctl_object_init(DRM_IOCTL_ARGS);
extern int  nouveau_ioctl_dma_object_init(DRM_IOCTL_ARGS);
extern uint32_t nouveau_chip_instance_get(drm_device_t *dev, struct mem_block *mem);

/* nouveau_irq.c */
extern irqreturn_t nouveau_irq_handler(DRM_IRQ_ARGS);
extern void        nouveau_irq_preinstall(drm_device_t*);
extern void        nouveau_irq_postinstall(drm_device_t*);
extern void        nouveau_irq_uninstall(drm_device_t*);

/* nv04_fb.c */
extern int  nv04_fb_init(drm_device_t *dev);
extern void nv04_fb_takedown(drm_device_t *dev);

/* nv10_fb.c */
extern int  nv10_fb_init(drm_device_t *dev);
extern void nv10_fb_takedown(drm_device_t *dev);

/* nv40_fb.c */
extern int  nv40_fb_init(drm_device_t *dev);
extern void nv40_fb_takedown(drm_device_t *dev);

/* nv04_graph.c */
extern void nouveau_nv04_context_switch(drm_device_t *dev);
extern int nv04_graph_init(drm_device_t *dev);
extern void nv04_graph_takedown(drm_device_t *dev);
extern int nv04_graph_context_create(drm_device_t *dev, int channel);

/* nv10_graph.c */
extern void nouveau_nv10_context_switch(drm_device_t *dev);
extern int nv10_graph_init(drm_device_t *dev);
extern void nv10_graph_takedown(drm_device_t *dev);
extern int nv10_graph_context_create(drm_device_t *dev, int channel);

/* nv20_graph.c */
extern void nouveau_nv20_context_switch(drm_device_t *dev);
extern int nv20_graph_init(drm_device_t *dev);
extern void nv20_graph_takedown(drm_device_t *dev);
extern int nv20_graph_context_create(drm_device_t *dev, int channel);

/* nv30_graph.c */
extern int nv30_graph_init(drm_device_t *dev);
extern void nv30_graph_takedown(drm_device_t *dev);
extern int nv30_graph_context_create(drm_device_t *dev, int channel);

/* nv40_graph.c */
extern int  nv40_graph_init(drm_device_t *dev);
extern void nv40_graph_takedown(drm_device_t *dev);
extern int  nv40_graph_context_create(drm_device_t *dev, int channel);
extern void nv40_graph_context_save_current(drm_device_t *dev);
extern void nv40_graph_context_restore(drm_device_t *dev, int channel);

/* nv04_mc.c */
extern int  nv04_mc_init(drm_device_t *dev);
extern void nv04_mc_takedown(drm_device_t *dev);

/* nv40_mc.c */
extern int  nv40_mc_init(drm_device_t *dev);
extern void nv40_mc_takedown(drm_device_t *dev);

/* nv04_timer.c */
extern int  nv04_timer_init(drm_device_t *dev);
extern void nv04_timer_takedown(drm_device_t *dev);

extern long nouveau_compat_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg);

#if defined(__powerpc__)
#define NV_READ(reg)        in_be32((void __iomem *)(dev_priv->mmio)->handle + (reg) )
#define NV_WRITE(reg,val)   out_be32((void __iomem *)(dev_priv->mmio)->handle + (reg) , (val) )
#else
#define NV_READ(reg)        DRM_READ32(  dev_priv->mmio, (reg) )
#define NV_WRITE(reg,val)   DRM_WRITE32( dev_priv->mmio, (reg), (val) )
#endif

#define INSTANCE_WR(mem,ofs,val) nouveau_instmem_w32(dev_priv,(mem),(ofs),(val))
#define INSTANCE_RD(mem,ofs)     nouveau_instmem_r32(dev_priv,(mem),(ofs))

#endif /* __NOUVEAU_DRV_H__ */


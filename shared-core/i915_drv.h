/* i915_drv.h -- Private header for the I915 driver -*- linux-c -*-
 */
/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _I915_DRV_H_
#define _I915_DRV_H_

/* General customization:
 */

#define DRIVER_AUTHOR		"Tungsten Graphics, Inc."

#define DRIVER_NAME		"i915"
#define DRIVER_DESC		"Intel Graphics"
#define DRIVER_DATE		"20070209"

#if defined(__linux__)
#define I915_HAVE_FENCE
#define I915_HAVE_BUFFER
#endif

/* Interface history:
 *
 * 1.1: Original.
 * 1.2: Add Power Management
 * 1.3: Add vblank support
 * 1.4: Fix cmdbuffer path, add heap destroy
 * 1.5: Add vblank pipe configuration
 * 1.6: - New ioctl for scheduling buffer swaps on vertical blank
 *      - Support vertical blank on secondary display pipe
 * 1.8: New ioctl for ARB_Occlusion_Query
 * 1.9: Usable page flipping and triple buffering
 * 1.10: Plane/pipe disentangling
 * 1.11: TTM superioctl
 * 1.12: TTM relocation optimization
 */
#define DRIVER_MAJOR		1
#if defined(I915_HAVE_FENCE) && defined(I915_HAVE_BUFFER)
#define DRIVER_MINOR		12
#else
#define DRIVER_MINOR		6
#endif
#define DRIVER_PATCHLEVEL	0

#ifdef I915_HAVE_BUFFER
#define I915_MAX_VALIDATE_BUFFERS 4096
#endif

struct drm_i915_ring_buffer {
	int tail_mask;
	unsigned long Start;
	unsigned long End;
	unsigned long Size;
	u8 *virtual_start;
	int head;
	int tail;
	int space;
	drm_local_map_t map;
};

struct mem_block {
	struct mem_block *next;
	struct mem_block *prev;
	int start;
	int size;
	struct drm_file *file_priv; /* NULL: free, -1: heap, other: real files */
};

struct drm_i915_vbl_swap {
	struct list_head head;
	drm_drawable_t drw_id;
	unsigned int plane;
	unsigned int sequence;
	int flip;
	struct drm_minor *minor;
};

struct drm_i915_master_private {
	drm_local_map_t *sarea;
	struct drm_i915_sarea *sarea_priv;
};
	
struct drm_i915_private {
	struct drm_buffer_object *ring_buffer;

	drm_local_map_t *mmio_map;

	unsigned long mmiobase;
	unsigned long mmiolen;

	struct drm_i915_ring_buffer ring;

	struct drm_dma_handle *status_page_dmah;
	void *hw_status_page;
	dma_addr_t dma_status_page;
	uint32_t counter;
	unsigned int status_gfx_addr;
	drm_local_map_t hws_map;

	unsigned int cpp;
	int use_mi_batchbuffer_start;

	wait_queue_head_t irq_queue;
	atomic_t irq_received;
	atomic_t irq_emitted;

	int tex_lru_log_granularity;
	int allow_batchbuffer;
	struct mem_block *agp_heap;
	unsigned int sr01, adpa, ppcr, dvob, dvoc, lvds;
	int vblank_pipe;
	DRM_SPINTYPE user_irq_lock;
	int user_irq_refcount;
	int fence_irq_on;
	uint32_t irq_enable_reg;
	int irq_enabled;
	struct workqueue_struct *wq;

	bool cursor_needs_physical;

#ifdef I915_HAVE_FENCE
	uint32_t flush_sequence;
	uint32_t flush_flags;
	uint32_t flush_pending;
	uint32_t saved_flush_status;
#endif
#ifdef I915_HAVE_BUFFER
	void *agp_iomap;
	unsigned int max_validate_buffers;
	struct mutex cmdbuf_mutex;
	size_t stolen_base;
#endif

	DRM_SPINTYPE swaps_lock;
	struct drm_i915_vbl_swap vbl_swaps;
	unsigned int swaps_pending;

	/* LVDS info */
	int backlight_duty_cycle;  /* restore backlight to this value */
	bool panel_wants_dither;
	struct drm_display_mode *panel_fixed_mode;

	/* DRI2 sarea */
	struct drm_buffer_object *sarea_bo;
	struct drm_bo_kmap_obj sarea_kmap;

	/* Register state */
	u8 saveLBB;
	u32 saveDSPACNTR;
	u32 saveDSPBCNTR;
	u32 savePIPEACONF;
	u32 savePIPEBCONF;
	u32 savePIPEASRC;
	u32 savePIPEBSRC;
	u32 saveFPA0;
	u32 saveFPA1;
	u32 saveDPLL_A;
	u32 saveDPLL_A_MD;
	u32 saveHTOTAL_A;
	u32 saveHBLANK_A;
	u32 saveHSYNC_A;
	u32 saveVTOTAL_A;
	u32 saveVBLANK_A;
	u32 saveVSYNC_A;
	u32 saveBCLRPAT_A;
	u32 savePIPEASTAT;
	u32 saveDSPASTRIDE;
	u32 saveDSPASIZE;
	u32 saveDSPAPOS;
	u32 saveDSPABASE;
	u32 saveDSPASURF;
	u32 saveDSPATILEOFF;
	u32 savePFIT_PGM_RATIOS;
	u32 saveBLC_PWM_CTL;
	u32 saveBLC_PWM_CTL2;
	u32 saveFPB0;
	u32 saveFPB1;
	u32 saveDPLL_B;
	u32 saveDPLL_B_MD;
	u32 saveHTOTAL_B;
	u32 saveHBLANK_B;
	u32 saveHSYNC_B;
	u32 saveVTOTAL_B;
	u32 saveVBLANK_B;
	u32 saveVSYNC_B;
	u32 saveBCLRPAT_B;
	u32 savePIPEBSTAT;
	u32 saveDSPBSTRIDE;
	u32 saveDSPBSIZE;
	u32 saveDSPBPOS;
	u32 saveDSPBBASE;
	u32 saveDSPBSURF;
	u32 saveDSPBTILEOFF;
	u32 saveVCLK_DIVISOR_VGA0;
	u32 saveVCLK_DIVISOR_VGA1;
	u32 saveVCLK_POST_DIV;
	u32 saveVGACNTRL;
	u32 saveADPA;
	u32 saveLVDS;
	u32 saveLVDSPP_ON;
	u32 saveLVDSPP_OFF;
	u32 saveDVOA;
	u32 saveDVOB;
	u32 saveDVOC;
	u32 savePP_ON;
	u32 savePP_OFF;
	u32 savePP_CONTROL;
	u32 savePP_CYCLE;
	u32 savePFIT_CONTROL;
	u32 save_palette_a[256];
	u32 save_palette_b[256];
	u32 saveFBC_CFB_BASE;
	u32 saveFBC_LL_BASE;
	u32 saveFBC_CONTROL;
	u32 saveFBC_CONTROL2;
	u32 saveIER;
	u32 saveIIR;
	u32 saveIMR;
	u32 saveCACHE_MODE_0;
	u32 saveDSPCLK_GATE_D;
	u32 saveMI_ARB_STATE;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF2[3];
	u8 saveMSR;
	u8 saveSR[8];
	u8 saveGR[25];
	u8 saveAR_INDEX;
	u8 saveAR[20];
	u8 saveDACMASK;
	u8 saveDACDATA[256*3]; /* 256 3-byte colors */
	u8 saveCR[36];
};

enum intel_chip_family {
	CHIP_I8XX = 0x01,
	CHIP_I9XX = 0x02,
	CHIP_I915 = 0x04,
	CHIP_I965 = 0x08,
};

extern struct drm_ioctl_desc i915_ioctls[];
extern int i915_max_ioctl;

extern int i915_master_create(struct drm_device *dev, struct drm_master *master);
extern void i915_master_destroy(struct drm_device *dev, struct drm_master *master);
				/* i915_dma.c */
extern void i915_kernel_lost_context(struct drm_device * dev);
extern int i915_driver_load(struct drm_device *, unsigned long flags);
extern int i915_driver_unload(struct drm_device *dev);
extern void i915_driver_lastclose(struct drm_device * dev);
extern void i915_driver_preclose(struct drm_device *dev,
				 struct drm_file *file_priv);
extern int i915_driver_device_is_agp(struct drm_device * dev);
extern long i915_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg);
extern void i915_emit_breadcrumb(struct drm_device *dev);
extern void i915_dispatch_flip(struct drm_device * dev, int pipes, int sync);
extern int i915_emit_mi_flush(struct drm_device *dev, uint32_t flush);
extern int i915_driver_firstopen(struct drm_device *dev);
extern int i915_do_cleanup_pageflip(struct drm_device *dev);
extern int i915_dma_cleanup(struct drm_device *dev);

/* i915_irq.c */
extern int i915_irq_emit(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int i915_irq_wait(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);

extern irqreturn_t i915_driver_irq_handler(DRM_IRQ_ARGS);
extern void i915_driver_irq_preinstall(struct drm_device * dev);
extern int i915_driver_irq_postinstall(struct drm_device * dev);
extern void i915_driver_irq_uninstall(struct drm_device * dev);
extern int i915_vblank_pipe_set(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
extern int i915_vblank_pipe_get(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
extern int i915_emit_irq(struct drm_device * dev);
extern void i915_enable_interrupt (struct drm_device *dev);
extern int i915_enable_vblank(struct drm_device *dev, int crtc);
extern void i915_disable_vblank(struct drm_device *dev, int crtc);
extern u32 i915_get_vblank_counter(struct drm_device *dev, int crtc);
extern int i915_vblank_swap(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
extern void i915_user_irq_on(struct drm_i915_private *dev_priv);
extern void i915_user_irq_off(struct drm_i915_private *dev_priv);

/* i915_mem.c */
extern int i915_mem_alloc(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
extern int i915_mem_free(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int i915_mem_init_heap(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
extern int i915_mem_destroy_heap(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
extern void i915_mem_takedown(struct mem_block **heap);
extern void i915_mem_release(struct drm_device * dev,
			     struct drm_file *file_priv,
			     struct mem_block *heap);
#ifdef I915_HAVE_FENCE
/* i915_fence.c */
extern void i915_fence_handler(struct drm_device *dev);
extern void i915_invalidate_reported_sequence(struct drm_device *dev);

#endif

#ifdef I915_HAVE_BUFFER
/* i915_buffer.c */
extern struct drm_ttm_backend *i915_create_ttm_backend_entry(struct drm_device *dev);
extern int i915_fence_type(struct drm_buffer_object *bo, uint32_t *fclass,
			   uint32_t *type);
extern int i915_invalidate_caches(struct drm_device *dev, uint64_t buffer_flags);
extern int i915_init_mem_type(struct drm_device *dev, uint32_t type,
			       struct drm_mem_type_manager *man);
extern uint64_t i915_evict_flags(struct drm_buffer_object *bo);
extern int i915_move(struct drm_buffer_object *bo, int evict,
		int no_wait, struct drm_bo_mem_reg *new_mem);
void i915_flush_ttm(struct drm_ttm *ttm);
#endif

#ifdef __linux__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
extern void intel_init_chipset_flush_compat(struct drm_device *dev);
extern void intel_fini_chipset_flush_compat(struct drm_device *dev);
#endif
#endif


/* modesetting */
extern void intel_modeset_init(struct drm_device *dev);
extern void intel_modeset_cleanup(struct drm_device *dev);


#define I915_READ(reg)          DRM_READ32(dev_priv->mmio_map, (reg))
#define I915_WRITE(reg,val)     DRM_WRITE32(dev_priv->mmio_map, (reg), (val))
#define I915_READ16(reg)	DRM_READ16(dev_priv->mmio_map, (reg))
#define I915_WRITE16(reg,val)	DRM_WRITE16(dev_priv->mmio_map, (reg), (val))

#define I915_VERBOSE 0

#define RING_LOCALS	unsigned int outring, ringmask, outcount; \
			volatile char *virt;

#define BEGIN_LP_RING(n) do {				\
	if (I915_VERBOSE)				\
		DRM_DEBUG("BEGIN_LP_RING(%d)\n",	\
	                         (n));		        \
	if (dev_priv->ring.space < (n)*4)                      \
		i915_wait_ring(dev, (n)*4, __FUNCTION__);      \
	outcount = 0;					\
	outring = dev_priv->ring.tail;			\
	ringmask = dev_priv->ring.tail_mask;		\
	virt = dev_priv->ring.virtual_start;		\
} while (0)

#define OUT_RING(n) do {					\
	if (I915_VERBOSE) DRM_DEBUG("   OUT_RING %x\n", (int)(n));	\
	*(volatile unsigned int *)(virt + outring) = (n);		\
	outcount++;						\
	outring += 4;						\
	outring &= ringmask;					\
} while (0)

#define ADVANCE_LP_RING() do {						\
	if (I915_VERBOSE) DRM_DEBUG("ADVANCE_LP_RING %x\n", outring);	\
	dev_priv->ring.tail = outring;					\
	dev_priv->ring.space -= outcount * 4;				\
	I915_WRITE(LP_RING + RING_TAIL, outring);			\
} while(0)

#define MI_NOOP	(0x00 << 23)

extern int i915_wait_ring(struct drm_device * dev, int n, const char *caller);

/*
 * The Bridge device's PCI config space has information about the
 * fb aperture size and the amount of pre-reserved memory.
 */
#define INTEL_GMCH_CTRL		0x52
#define INTEL_GMCH_ENABLED	0x4
#define INTEL_GMCH_MEM_MASK	0x1
#define INTEL_GMCH_MEM_64M	0x1
#define INTEL_GMCH_MEM_128M	0

#define INTEL_855_GMCH_GMS_MASK		(0x7 << 4)
#define INTEL_855_GMCH_GMS_DISABLED	(0x0 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_1M	(0x1 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_4M	(0x2 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_8M	(0x3 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_16M	(0x4 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_32M	(0x5 << 4)

#define INTEL_915G_GMCH_GMS_STOLEN_48M	(0x6 << 4)
#define INTEL_915G_GMCH_GMS_STOLEN_64M	(0x7 << 4)

/* Extended config space */
#define LBB 0xf4

/* VGA stuff */

#define VGA_ST01_MDA 0x3ba
#define VGA_ST01_CGA 0x3da

#define VGA_MSR_WRITE 0x3c2
#define VGA_MSR_READ 0x3cc
#define   VGA_MSR_MEM_EN (1<<1)
#define   VGA_MSR_CGA_MODE (1<<0)

#define VGA_SR_INDEX 0x3c4
#define VGA_SR_DATA 0x3c5

#define VGA_AR_INDEX 0x3c0
#define   VGA_AR_VID_EN (1<<5)
#define VGA_AR_DATA_WRITE 0x3c0
#define VGA_AR_DATA_READ 0x3c1

#define VGA_GR_INDEX 0x3ce
#define VGA_GR_DATA 0x3cf
/* GR05 */
#define   VGA_GR_MEM_READ_MODE_SHIFT 3
#define     VGA_GR_MEM_READ_MODE_PLANE 1
/* GR06 */
#define   VGA_GR_MEM_MODE_MASK 0xc
#define   VGA_GR_MEM_MODE_SHIFT 2
#define   VGA_GR_MEM_A0000_AFFFF 0
#define   VGA_GR_MEM_A0000_BFFFF 1
#define   VGA_GR_MEM_B0000_B7FFF 2
#define   VGA_GR_MEM_B0000_BFFFF 3

#define VGA_DACMASK 0x3c6
#define VGA_DACRX 0x3c7
#define VGA_DACWX 0x3c8
#define VGA_DACDATA 0x3c9

#define VGA_CR_INDEX_MDA 0x3b4
#define VGA_CR_DATA_MDA 0x3b5
#define VGA_CR_INDEX_CGA 0x3d4
#define VGA_CR_DATA_CGA 0x3d5

#define GFX_OP_USER_INTERRUPT		((0<<29)|(2<<23))
#define GFX_OP_BREAKPOINT_INTERRUPT	((0<<29)|(1<<23))
#define CMD_REPORT_HEAD			(7<<23)
#define CMD_STORE_DWORD_IDX		((0x21<<23) | 0x1)
#define CMD_OP_BATCH_BUFFER  ((0x0<<29)|(0x30<<23)|0x1)

#define CMD_MI_FLUSH         (0x04 << 23)
#define MI_NO_WRITE_FLUSH    (1 << 2)
#define MI_READ_FLUSH        (1 << 0)
#define MI_EXE_FLUSH         (1 << 1)
#define MI_END_SCENE         (1 << 4) /* flush binner and incr scene count */
#define MI_SCENE_COUNT       (1 << 3) /* just increment scene count */

/* Packet to load a register value from the ring/batch command stream:
 */
#define CMD_MI_LOAD_REGISTER_IMM	((0x22 << 23)|0x1)

#define BB1_START_ADDR_MASK   (~0x7)
#define BB1_PROTECTED         (1<<0)
#define BB1_UNPROTECTED       (0<<0)
#define BB2_END_ADDR_MASK     (~0x7)

#define I915REG_HWS_PGA		0x02080

/* Framebuffer compression */
#define FBC_CFB_BASE		0x03200 /* 4k page aligned */
#define FBC_LL_BASE		0x03204 /* 4k page aligned */
#define FBC_CONTROL		0x03208
#define   FBC_CTL_EN		(1<<31)
#define   FBC_CTL_PERIODIC	(1<<30)
#define   FBC_CTL_INTERVAL_SHIFT (16)
#define   FBC_CTL_UNCOMPRESSIBLE (1<<14)
#define   FBC_CTL_STRIDE_SHIFT	(5)
#define   FBC_CTL_FENCENO	(1<<0)
#define FBC_COMMAND		0x0320c
#define   FBC_CMD_COMPRESS	(1<<0)
#define FBC_STATUS		0x03210
#define   FBC_STAT_COMPRESSING	(1<<31)
#define   FBC_STAT_COMPRESSED	(1<<30)
#define   FBC_STAT_MODIFIED	(1<<29)
#define   FBC_STAT_CURRENT_LINE	(1<<0)
#define FBC_CONTROL2		0x03214
#define   FBC_CTL_FENCE_DBL	(0<<4)
#define   FBC_CTL_IDLE_IMM	(0<<2)
#define   FBC_CTL_IDLE_FULL	(1<<2)
#define   FBC_CTL_IDLE_LINE	(2<<2)
#define   FBC_CTL_IDLE_DEBUG	(3<<2)
#define   FBC_CTL_CPU_FENCE	(1<<1)
#define   FBC_CTL_PLANEA	(0<<0)
#define   FBC_CTL_PLANEB	(1<<0)
#define FBC_FENCE_OFF		0x0321b

#define FBC_LL_SIZE		(1536)
#define FBC_LL_PAD		(32)

/* Interrupt bits:
 */
#define USER_INT_FLAG    (1<<1)
#define VSYNC_PIPEB_FLAG (1<<5)
#define VSYNC_PIPEA_FLAG (1<<7)
#define HWB_OOM_FLAG     (1<<13) /* binner out of memory */

#define I915REG_HWSTAM		0x02098
#define I915REG_INT_IDENTITY_R	0x020a4
#define I915REG_INT_MASK_R	0x020a8
#define I915REG_INT_ENABLE_R	0x020a0
#define I915REG_INSTPM	        0x020c0

#define PIPEADSL		0x70000
#define PIPEBDSL		0x71000

#define I915REG_PIPEASTAT	0x70024
#define I915REG_PIPEBSTAT	0x71024
/*
 * The two pipe frame counter registers are not synchronized, so
 * reading a stable value is somewhat tricky. The following code 
 * should work:
 *
 *  do {
 *    high1 = ((INREG(PIPEAFRAMEHIGH) & PIPE_FRAME_HIGH_MASK) >>
 *             PIPE_FRAME_HIGH_SHIFT;
 *    low1 =  ((INREG(PIPEAFRAMEPIXEL) & PIPE_FRAME_LOW_MASK) >>
 *             PIPE_FRAME_LOW_SHIFT);
 *    high2 = ((INREG(PIPEAFRAMEHIGH) & PIPE_FRAME_HIGH_MASK) >>
 *             PIPE_FRAME_HIGH_SHIFT);
 *  } while (high1 != high2);
 *  frame = (high1 << 8) | low1;
 */
#define PIPEAFRAMEHIGH          0x70040
#define PIPEBFRAMEHIGH		0x71040
#define PIPE_FRAME_HIGH_MASK    0x0000ffff
#define PIPE_FRAME_HIGH_SHIFT   0
#define PIPEAFRAMEPIXEL         0x70044
#define PIPEBFRAMEPIXEL		0x71044

#define PIPE_FRAME_LOW_MASK     0xff000000
#define PIPE_FRAME_LOW_SHIFT    24
/*
 * Pixel within the current frame is counted in the PIPEAFRAMEPIXEL register
 * and is 24 bits wide.
 */
#define PIPE_PIXEL_MASK         0x00ffffff
#define PIPE_PIXEL_SHIFT        0

#define I915_VBLANK_INTERRUPT_ENABLE	(1UL<<17)
#define I915_VBLANK_CLEAR		(1UL<<1)

#define GPIOA			0x5010
#define GPIOB			0x5014
#define GPIOC			0x5018
#define GPIOD			0x501c
#define GPIOE			0x5020
#define GPIOF			0x5024
#define GPIOG			0x5028
#define GPIOH			0x502c
# define GPIO_CLOCK_DIR_MASK		(1 << 0)
# define GPIO_CLOCK_DIR_IN		(0 << 1)
# define GPIO_CLOCK_DIR_OUT		(1 << 1)
# define GPIO_CLOCK_VAL_MASK		(1 << 2)
# define GPIO_CLOCK_VAL_OUT		(1 << 3)
# define GPIO_CLOCK_VAL_IN		(1 << 4)
# define GPIO_CLOCK_PULLUP_DISABLE	(1 << 5)
# define GPIO_DATA_DIR_MASK		(1 << 8)
# define GPIO_DATA_DIR_IN		(0 << 9)
# define GPIO_DATA_DIR_OUT		(1 << 9)
# define GPIO_DATA_VAL_MASK		(1 << 10)
# define GPIO_DATA_VAL_OUT		(1 << 11)
# define GPIO_DATA_VAL_IN		(1 << 12)
# define GPIO_DATA_PULLUP_DISABLE	(1 << 13)

/* p317, 319
 */
#define VCLK2_VCO_M        0x6008 /* treat as 16 bit? (includes msbs) */
#define VCLK2_VCO_N        0x600a
#define VCLK2_VCO_DIV_SEL  0x6012

#define VCLK_DIVISOR_VGA0   0x6000
#define VCLK_DIVISOR_VGA1   0x6004
#define VCLK_POST_DIV	    0x6010
/** Selects a post divisor of 4 instead of 2. */
# define VGA1_PD_P2_DIV_4	(1 << 15)
/** Overrides the p2 post divisor field */
# define VGA1_PD_P1_DIV_2	(1 << 13)
# define VGA1_PD_P1_SHIFT	8
/** P1 value is 2 greater than this field */
# define VGA1_PD_P1_MASK	(0x1f << 8)
/** Selects a post divisor of 4 instead of 2. */
# define VGA0_PD_P2_DIV_4	(1 << 7)
/** Overrides the p2 post divisor field */
# define VGA0_PD_P1_DIV_2	(1 << 5)
# define VGA0_PD_P1_SHIFT	0
/** P1 value is 2 greater than this field */
# define VGA0_PD_P1_MASK	(0x1f << 0)

#define POST_DIV_SELECT        0x70
#define POST_DIV_1             0x00
#define POST_DIV_2             0x10
#define POST_DIV_4             0x20
#define POST_DIV_8             0x30
#define POST_DIV_16            0x40
#define POST_DIV_32            0x50
#define VCO_LOOP_DIV_BY_4M     0x00
#define VCO_LOOP_DIV_BY_16M    0x04

#define SRX_INDEX		0x3c4
#define SRX_DATA		0x3c5
#define SR01			1
#define SR01_SCREEN_OFF		(1<<5)

#define PPCR			0x61204
#define PPCR_ON			(1<<0)

#define DVOA			0x61120
#define DVOA_ON			(1<<31)
#define DVOB			0x61140
#define DVOB_ON			(1<<31)
#define DVOC			0x61160
#define DVOC_ON			(1<<31)
#define LVDS			0x61180
#define LVDS_ON			(1<<31)

#define ADPA			0x61100
#define ADPA_DPMS_MASK		(~(3<<10))
#define ADPA_DPMS_ON		(0<<10)
#define ADPA_DPMS_SUSPEND	(1<<10)
#define ADPA_DPMS_STANDBY	(2<<10)
#define ADPA_DPMS_OFF		(3<<10)

#define NOPID                   0x2094
#define LP_RING			0x2030
#define HP_RING			0x2040
/* The binner has its own ring buffer:
 */
#define HWB_RING		0x2400

#define RING_TAIL		0x00
#define TAIL_ADDR		0x001FFFF8
#define RING_HEAD		0x04
#define HEAD_WRAP_COUNT		0xFFE00000
#define HEAD_WRAP_ONE		0x00200000
#define HEAD_ADDR		0x001FFFFC
#define RING_START		0x08
#define START_ADDR		0x0xFFFFF000
#define RING_LEN		0x0C
#define RING_NR_PAGES		0x001FF000
#define RING_REPORT_MASK	0x00000006
#define RING_REPORT_64K		0x00000002
#define RING_REPORT_128K	0x00000004
#define RING_NO_REPORT		0x00000000
#define RING_VALID_MASK		0x00000001
#define RING_VALID		0x00000001
#define RING_INVALID		0x00000000

/* Instruction parser error reg:
 */
#define IPEIR			0x2088

/* Scratch pad debug 0 reg:
 */
#define SCPD0			0x209c

/* Error status reg:
 */
#define ESR			0x20b8

/* Secondary DMA fetch address debug reg:
 */
#define DMA_FADD_S		0x20d4

/* Memory Interface Arbitration State
 */
#define MI_ARB_STATE		0x20e4

/* Cache mode 0 reg.
 *  - Manipulating render cache behaviour is central
 *    to the concept of zone rendering, tuning this reg can help avoid
 *    unnecessary render cache reads and even writes (for z/stencil)
 *    at beginning and end of scene.
 *
 * - To change a bit, write to this reg with a mask bit set and the
 * bit of interest either set or cleared.  EG: (BIT<<16) | BIT to set.
 */
#define Cache_Mode_0		0x2120
#define CACHE_MODE_0		0x2120
#define CM0_MASK_SHIFT          16
#define CM0_IZ_OPT_DISABLE      (1<<6)
#define CM0_ZR_OPT_DISABLE      (1<<5)
#define CM0_DEPTH_EVICT_DISABLE (1<<4)
#define CM0_COLOR_EVICT_DISABLE (1<<3)
#define CM0_DEPTH_WRITE_DISABLE (1<<1)
#define CM0_RC_OP_FLUSH_DISABLE (1<<0)


/* Graphics flush control.  A CPU write flushes the GWB of all writes.
 * The data is discarded.
 */
#define GFX_FLSH_CNTL		0x2170

/* Binner control.  Defines the location of the bin pointer list:
 */
#define BINCTL			0x2420
#define BC_MASK			(1 << 9)

/* Binned scene info.
 */
#define BINSCENE		0x2428
#define BS_OP_LOAD		(1 << 8)
#define BS_MASK			(1 << 22)

/* Bin command parser debug reg:
 */
#define BCPD			0x2480

/* Bin memory control debug reg:
 */
#define BMCD			0x2484

/* Bin data cache debug reg:
 */
#define BDCD			0x2488

/* Binner pointer cache debug reg:
 */
#define BPCD			0x248c

/* Binner scratch pad debug reg:
 */
#define BINSKPD			0x24f0

/* HWB scratch pad debug reg:
 */
#define HWBSKPD			0x24f4

/* Binner memory pool reg:
 */
#define BMP_BUFFER		0x2430
#define BMP_PAGE_SIZE_4K	(0 << 10)
#define BMP_BUFFER_SIZE_SHIFT	1
#define BMP_ENABLE		(1 << 0)

/* Get/put memory from the binner memory pool:
 */
#define BMP_GET			0x2438
#define BMP_PUT			0x2440
#define BMP_OFFSET_SHIFT	5

/* 3D state packets:
 */
#define GFX_OP_RASTER_RULES    ((0x3<<29)|(0x7<<24))

#define GFX_OP_SCISSOR         ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define SC_UPDATE_SCISSOR       (0x1<<1)
#define SC_ENABLE_MASK          (0x1<<0)
#define SC_ENABLE               (0x1<<0)

#define GFX_OP_LOAD_INDIRECT   ((0x3<<29)|(0x1d<<24)|(0x7<<16))

#define GFX_OP_SCISSOR_INFO    ((0x3<<29)|(0x1d<<24)|(0x81<<16)|(0x1))
#define SCI_YMIN_MASK      (0xffff<<16)
#define SCI_XMIN_MASK      (0xffff<<0)
#define SCI_YMAX_MASK      (0xffff<<16)
#define SCI_XMAX_MASK      (0xffff<<0)

#define GFX_OP_SCISSOR_ENABLE	 ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define GFX_OP_SCISSOR_RECT	 ((0x3<<29)|(0x1d<<24)|(0x81<<16)|1)
#define GFX_OP_COLOR_FACTOR      ((0x3<<29)|(0x1d<<24)|(0x1<<16)|0x0)
#define GFX_OP_STIPPLE           ((0x3<<29)|(0x1d<<24)|(0x83<<16))
#define GFX_OP_MAP_INFO          ((0x3<<29)|(0x1d<<24)|0x4)
#define GFX_OP_DESTBUFFER_VARS   ((0x3<<29)|(0x1d<<24)|(0x85<<16)|0x0)
#define GFX_OP_DRAWRECT_INFO     ((0x3<<29)|(0x1d<<24)|(0x80<<16)|(0x3))

#define GFX_OP_DRAWRECT_INFO_I965  ((0x7900<<16)|0x2)

#define SRC_COPY_BLT_CMD                ((2<<29)|(0x43<<22)|4)
#define XY_SRC_COPY_BLT_CMD		((2<<29)|(0x53<<22)|6)
#define XY_MONO_SRC_COPY_IMM_BLT	((2<<29)|(0x71<<22)|5)
#define XY_SRC_COPY_BLT_WRITE_ALPHA	(1<<21)
#define XY_SRC_COPY_BLT_WRITE_RGB	(1<<20)
#define   BLT_DEPTH_8			(0<<24)
#define   BLT_DEPTH_16_565		(1<<24)
#define   BLT_DEPTH_16_1555		(2<<24)
#define   BLT_DEPTH_32			(3<<24)
#define   BLT_ROP_GXCOPY		(0xcc<<16)

#define MI_BATCH_BUFFER		((0x30<<23)|1)
#define MI_BATCH_BUFFER_START	(0x31<<23)
#define MI_BATCH_BUFFER_END	(0xA<<23)
#define MI_BATCH_NON_SECURE	(1)

#define MI_BATCH_NON_SECURE_I965 (1<<8)

#define MI_WAIT_FOR_EVENT       ((0x3<<23))
#define MI_WAIT_FOR_PLANE_B_FLIP      (1<<6)
#define MI_WAIT_FOR_PLANE_A_FLIP      (1<<2)
#define MI_WAIT_FOR_PLANE_A_SCANLINES (1<<1)

#define MI_LOAD_SCAN_LINES_INCL  ((0x12<<23))

#define CMD_OP_DISPLAYBUFFER_INFO ((0x0<<29)|(0x14<<23)|2)
#define ASYNC_FLIP                (1<<22)
#define DISPLAY_PLANE_A           (0<<20)
#define DISPLAY_PLANE_B           (1<<20)

/* Display regs */
#define DSPACNTR                0x70180
#define DSPBCNTR                0x71180
#define DISPPLANE_SEL_PIPE_MASK                 (1<<24)

/* Define the region of interest for the binner:
 */
#define CMD_OP_BIN_CONTROL	 ((0x3<<29)|(0x1d<<24)|(0x84<<16)|4)

#define CMD_OP_DESTBUFFER_INFO	 ((0x3<<29)|(0x1d<<24)|(0x8e<<16)|1)

#define BREADCRUMB_BITS 31
#define BREADCRUMB_MASK ((1U << BREADCRUMB_BITS) - 1)

#define READ_BREADCRUMB(dev_priv)  (((volatile u32*)(dev_priv->hw_status_page))[5])
#define READ_HWSP(dev_priv, reg)  (((volatile u32*)(dev_priv->hw_status_page))[reg])

#define BLC_PWM_CTL		0x61254
#define BACKLIGHT_MODULATION_FREQ_SHIFT		(17)

#define BLC_PWM_CTL2		0x61250

/**
 * This is the most significant 15 bits of the number of backlight cycles in a
 * complete cycle of the modulated backlight control.
 *
 * The actual value is this field multiplied by two.
 */
#define BACKLIGHT_MODULATION_FREQ_MASK		(0x7fff << 17)
#define BLM_LEGACY_MODE				(1 << 16)
/**
 * This is the number of cycles out of the backlight modulation cycle for which
 * the backlight is on.
 *
 * This field must be no greater than the number of cycles in the complete
 * backlight modulation cycle.
 */
#define BACKLIGHT_DUTY_CYCLE_SHIFT		(0)
#define BACKLIGHT_DUTY_CYCLE_MASK		(0xffff)

#define I915_GCFGC			0xf0
#define I915_LOW_FREQUENCY_ENABLE		(1 << 7)
#define I915_DISPLAY_CLOCK_190_200_MHZ		(0 << 4)
#define I915_DISPLAY_CLOCK_333_MHZ		(4 << 4)
#define I915_DISPLAY_CLOCK_MASK			(7 << 4)

#define I855_HPLLCC			0xc0
#define I855_CLOCK_CONTROL_MASK			(3 << 0)
#define I855_CLOCK_133_200			(0 << 0)
#define I855_CLOCK_100_200			(1 << 0)
#define I855_CLOCK_100_133			(2 << 0)
#define I855_CLOCK_166_250			(3 << 0)

/* p317, 319
 */
#define VCLK2_VCO_M        0x6008 /* treat as 16 bit? (includes msbs) */
#define VCLK2_VCO_N        0x600a
#define VCLK2_VCO_DIV_SEL  0x6012

#define VCLK_DIVISOR_VGA0   0x6000
#define VCLK_DIVISOR_VGA1   0x6004
#define VCLK_POST_DIV	    0x6010
/** Selects a post divisor of 4 instead of 2. */
# define VGA1_PD_P2_DIV_4	(1 << 15)
/** Overrides the p2 post divisor field */
# define VGA1_PD_P1_DIV_2	(1 << 13)
# define VGA1_PD_P1_SHIFT	8
/** P1 value is 2 greater than this field */
# define VGA1_PD_P1_MASK	(0x1f << 8)
/** Selects a post divisor of 4 instead of 2. */
# define VGA0_PD_P2_DIV_4	(1 << 7)
/** Overrides the p2 post divisor field */
# define VGA0_PD_P1_DIV_2	(1 << 5)
# define VGA0_PD_P1_SHIFT	0
/** P1 value is 2 greater than this field */
# define VGA0_PD_P1_MASK	(0x1f << 0)

#define DSPCLK_GATE_D	0x6200

/* I830 CRTC registers */
#define HTOTAL_A	0x60000
#define HBLANK_A	0x60004
#define HSYNC_A		0x60008
#define VTOTAL_A	0x6000c
#define VBLANK_A	0x60010
#define VSYNC_A		0x60014
#define PIPEASRC	0x6001c
#define BCLRPAT_A	0x60020
#define VSYNCSHIFT_A	0x60028

#define HTOTAL_B	0x61000
#define HBLANK_B	0x61004
#define HSYNC_B		0x61008
#define VTOTAL_B	0x6100c
#define VBLANK_B	0x61010
#define VSYNC_B		0x61014
#define PIPEBSRC	0x6101c
#define BCLRPAT_B	0x61020
#define VSYNCSHIFT_B	0x61028

#define HACTIVE_MASK	0x00000fff
#define VTOTAL_MASK	0x00001fff
#define VTOTAL_SHIFT	16
#define VACTIVE_MASK	0x00000fff
#define VBLANK_END_MASK	0x00001fff
#define VBLANK_END_SHIFT 16
#define VBLANK_START_MASK 0x00001fff

#define PP_STATUS	0x61200
# define PP_ON					(1 << 31)
/**
 * Indicates that all dependencies of the panel are on:
 *
 * - PLL enabled
 * - pipe enabled
 * - LVDS/DVOB/DVOC on
 */
# define PP_READY				(1 << 30)
# define PP_SEQUENCE_NONE			(0 << 28)
# define PP_SEQUENCE_ON				(1 << 28)
# define PP_SEQUENCE_OFF			(2 << 28)
# define PP_SEQUENCE_MASK			0x30000000
#define PP_CONTROL	0x61204
# define POWER_TARGET_ON			(1 << 0)

#define LVDSPP_ON       0x61208
#define LVDSPP_OFF      0x6120c
#define PP_CYCLE        0x61210

#define PFIT_CONTROL	0x61230
# define PFIT_ENABLE				(1 << 31)
# define PFIT_PIPE_MASK				(3 << 29)
# define PFIT_PIPE_SHIFT			29
# define VERT_INTERP_DISABLE			(0 << 10)
# define VERT_INTERP_BILINEAR			(1 << 10)
# define VERT_INTERP_MASK			(3 << 10)
# define VERT_AUTO_SCALE			(1 << 9)
# define HORIZ_INTERP_DISABLE			(0 << 6)
# define HORIZ_INTERP_BILINEAR			(1 << 6)
# define HORIZ_INTERP_MASK			(3 << 6)
# define HORIZ_AUTO_SCALE			(1 << 5)
# define PANEL_8TO6_DITHER_ENABLE		(1 << 3)

#define PFIT_PGM_RATIOS	0x61234
# define PFIT_VERT_SCALE_MASK			0xfff00000
# define PFIT_HORIZ_SCALE_MASK			0x0000fff0

#define PFIT_AUTO_RATIOS	0x61238


#define DPLL_A		0x06014
#define DPLL_B		0x06018
# define DPLL_VCO_ENABLE			(1 << 31)
# define DPLL_DVO_HIGH_SPEED			(1 << 30)
# define DPLL_SYNCLOCK_ENABLE			(1 << 29)
# define DPLL_VGA_MODE_DIS			(1 << 28)
# define DPLLB_MODE_DAC_SERIAL			(1 << 26) /* i915 */
# define DPLLB_MODE_LVDS			(2 << 26) /* i915 */
# define DPLL_MODE_MASK				(3 << 26)
# define DPLL_DAC_SERIAL_P2_CLOCK_DIV_10	(0 << 24) /* i915 */
# define DPLL_DAC_SERIAL_P2_CLOCK_DIV_5		(1 << 24) /* i915 */
# define DPLLB_LVDS_P2_CLOCK_DIV_14		(0 << 24) /* i915 */
# define DPLLB_LVDS_P2_CLOCK_DIV_7		(1 << 24) /* i915 */
# define DPLL_P2_CLOCK_DIV_MASK			0x03000000 /* i915 */
# define DPLL_FPA01_P1_POST_DIV_MASK		0x00ff0000 /* i915 */
/**
 *  The i830 generation, in DAC/serial mode, defines p1 as two plus this
 * bitfield, or just 2 if PLL_P1_DIVIDE_BY_TWO is set.
 */
# define DPLL_FPA01_P1_POST_DIV_MASK_I830	0x001f0000
/**
 * The i830 generation, in LVDS mode, defines P1 as the bit number set within
 * this field (only one bit may be set).
 */
# define DPLL_FPA01_P1_POST_DIV_MASK_I830_LVDS	0x003f0000
# define DPLL_FPA01_P1_POST_DIV_SHIFT		16
# define PLL_P2_DIVIDE_BY_4			(1 << 23) /* i830, required in DVO non-gang */
# define PLL_P1_DIVIDE_BY_TWO			(1 << 21) /* i830 */
# define PLL_REF_INPUT_DREFCLK			(0 << 13)
# define PLL_REF_INPUT_TVCLKINA			(1 << 13) /* i830 */
# define PLL_REF_INPUT_TVCLKINBC		(2 << 13) /* SDVO TVCLKIN */
# define PLLB_REF_INPUT_SPREADSPECTRUMIN	(3 << 13)
# define PLL_REF_INPUT_MASK			(3 << 13)
# define PLL_LOAD_PULSE_PHASE_SHIFT		9
/*
 * Parallel to Serial Load Pulse phase selection.
 * Selects the phase for the 10X DPLL clock for the PCIe
 * digital display port. The range is 4 to 13; 10 or more
 * is just a flip delay. The default is 6
 */
# define PLL_LOAD_PULSE_PHASE_MASK		(0xf << PLL_LOAD_PULSE_PHASE_SHIFT)
# define DISPLAY_RATE_SELECT_FPA1		(1 << 8)

/**
 * SDVO multiplier for 945G/GM. Not used on 965.
 *
 * \sa DPLL_MD_UDI_MULTIPLIER_MASK
 */
# define SDVO_MULTIPLIER_MASK			0x000000ff
# define SDVO_MULTIPLIER_SHIFT_HIRES		4
# define SDVO_MULTIPLIER_SHIFT_VGA		0

/** @defgroup DPLL_MD
 * @{
 */
/** Pipe A SDVO/UDI clock multiplier/divider register for G965. */
#define DPLL_A_MD		0x0601c
/** Pipe B SDVO/UDI clock multiplier/divider register for G965. */
#define DPLL_B_MD		0x06020
/**
 * UDI pixel divider, controlling how many pixels are stuffed into a packet.
 *
 * Value is pixels minus 1.  Must be set to 1 pixel for SDVO.
 */
# define DPLL_MD_UDI_DIVIDER_MASK		0x3f000000
# define DPLL_MD_UDI_DIVIDER_SHIFT		24
/** UDI pixel divider for VGA, same as DPLL_MD_UDI_DIVIDER_MASK. */
# define DPLL_MD_VGA_UDI_DIVIDER_MASK		0x003f0000
# define DPLL_MD_VGA_UDI_DIVIDER_SHIFT		16
/**
 * SDVO/UDI pixel multiplier.
 *
 * SDVO requires that the bus clock rate be between 1 and 2 Ghz, and the bus
 * clock rate is 10 times the DPLL clock.  At low resolution/refresh rate
 * modes, the bus rate would be below the limits, so SDVO allows for stuffing
 * dummy bytes in the datastream at an increased clock rate, with both sides of
 * the link knowing how many bytes are fill.
 *
 * So, for a mode with a dotclock of 65Mhz, we would want to double the clock
 * rate to 130Mhz to get a bus rate of 1.30Ghz.  The DPLL clock rate would be
 * set to 130Mhz, and the SDVO multiplier set to 2x in this register and
 * through an SDVO command.
 *
 * This register field has values of multiplication factor minus 1, with
 * a maximum multiplier of 5 for SDVO.
 */
# define DPLL_MD_UDI_MULTIPLIER_MASK		0x00003f00
# define DPLL_MD_UDI_MULTIPLIER_SHIFT		8
/** SDVO/UDI pixel multiplier for VGA, same as DPLL_MD_UDI_MULTIPLIER_MASK.
 * This best be set to the default value (3) or the CRT won't work. No,
 * I don't entirely understand what this does...
 */
# define DPLL_MD_VGA_UDI_MULTIPLIER_MASK	0x0000003f
# define DPLL_MD_VGA_UDI_MULTIPLIER_SHIFT	0
/** @} */

#define DPLL_TEST		0x606c
# define DPLLB_TEST_SDVO_DIV_1			(0 << 22)
# define DPLLB_TEST_SDVO_DIV_2			(1 << 22)
# define DPLLB_TEST_SDVO_DIV_4			(2 << 22)
# define DPLLB_TEST_SDVO_DIV_MASK		(3 << 22)
# define DPLLB_TEST_N_BYPASS			(1 << 19)
# define DPLLB_TEST_M_BYPASS			(1 << 18)
# define DPLLB_INPUT_BUFFER_ENABLE		(1 << 16)
# define DPLLA_TEST_N_BYPASS			(1 << 3)
# define DPLLA_TEST_M_BYPASS			(1 << 2)
# define DPLLA_INPUT_BUFFER_ENABLE		(1 << 0)

#define ADPA			0x61100
#define ADPA_DAC_ENABLE		(1<<31)
#define ADPA_DAC_DISABLE	0
#define ADPA_PIPE_SELECT_MASK	(1<<30)
#define ADPA_PIPE_A_SELECT	0
#define ADPA_PIPE_B_SELECT	(1<<30)
#define ADPA_USE_VGA_HVPOLARITY (1<<15)
#define ADPA_SETS_HVPOLARITY	0
#define ADPA_VSYNC_CNTL_DISABLE (1<<11)
#define ADPA_VSYNC_CNTL_ENABLE	0
#define ADPA_HSYNC_CNTL_DISABLE (1<<10)
#define ADPA_HSYNC_CNTL_ENABLE	0
#define ADPA_VSYNC_ACTIVE_HIGH	(1<<4)
#define ADPA_VSYNC_ACTIVE_LOW	0
#define ADPA_HSYNC_ACTIVE_HIGH	(1<<3)
#define ADPA_HSYNC_ACTIVE_LOW	0

#define FPA0		0x06040
#define FPA1		0x06044
#define FPB0		0x06048
#define FPB1		0x0604c
# define FP_N_DIV_MASK				0x003f0000
# define FP_N_DIV_SHIFT				16
# define FP_M1_DIV_MASK				0x00003f00
# define FP_M1_DIV_SHIFT			8
# define FP_M2_DIV_MASK				0x0000003f
# define FP_M2_DIV_SHIFT			0


#define PORT_HOTPLUG_EN		0x61110
# define SDVOB_HOTPLUG_INT_EN			(1 << 26)
# define SDVOC_HOTPLUG_INT_EN			(1 << 25)
# define TV_HOTPLUG_INT_EN			(1 << 18)
# define CRT_HOTPLUG_INT_EN			(1 << 9)
# define CRT_HOTPLUG_FORCE_DETECT		(1 << 3)

#define PORT_HOTPLUG_STAT	0x61114
# define CRT_HOTPLUG_INT_STATUS			(1 << 11)
# define TV_HOTPLUG_INT_STATUS			(1 << 10)
# define CRT_HOTPLUG_MONITOR_MASK		(3 << 8)
# define CRT_HOTPLUG_MONITOR_COLOR		(3 << 8)
# define CRT_HOTPLUG_MONITOR_MONO		(2 << 8)
# define CRT_HOTPLUG_MONITOR_NONE		(0 << 8)
# define SDVOC_HOTPLUG_INT_STATUS		(1 << 7)
# define SDVOB_HOTPLUG_INT_STATUS		(1 << 6)

#define SDVOB			0x61140
#define SDVOC			0x61160
#define SDVO_ENABLE				(1 << 31)
#define SDVO_PIPE_B_SELECT			(1 << 30)
#define SDVO_STALL_SELECT			(1 << 29)
#define SDVO_INTERRUPT_ENABLE			(1 << 26)
/**
 * 915G/GM SDVO pixel multiplier.
 *
 * Programmed value is multiplier - 1, up to 5x.
 *
 * \sa DPLL_MD_UDI_MULTIPLIER_MASK
 */
#define SDVO_PORT_MULTIPLY_MASK			(7 << 23)
#define SDVO_PORT_MULTIPLY_SHIFT		23
#define SDVO_PHASE_SELECT_MASK			(15 << 19)
#define SDVO_PHASE_SELECT_DEFAULT		(6 << 19)
#define SDVO_CLOCK_OUTPUT_INVERT		(1 << 18)
#define SDVOC_GANG_MODE				(1 << 16)
#define SDVO_BORDER_ENABLE			(1 << 7)
#define SDVOB_PCIE_CONCURRENCY			(1 << 3)
#define SDVO_DETECTED				(1 << 2)
/* Bits to be preserved when writing */
#define SDVOB_PRESERVE_MASK			((1 << 17) | (1 << 16) | (1 << 14))
#define SDVOC_PRESERVE_MASK			(1 << 17)

/** @defgroup LVDS
 * @{
 */
/**
 * This register controls the LVDS output enable, pipe selection, and data
 * format selection.
 *
 * All of the clock/data pairs are force powered down by power sequencing.
 */
#define LVDS			0x61180
/**
 * Enables the LVDS port.  This bit must be set before DPLLs are enabled, as
 * the DPLL semantics change when the LVDS is assigned to that pipe.
 */
# define LVDS_PORT_EN			(1 << 31)
/** Selects pipe B for LVDS data.  Must be set on pre-965. */
# define LVDS_PIPEB_SELECT		(1 << 30)

/**
 * Enables the A0-A2 data pairs and CLKA, containing 18 bits of color data per
 * pixel.
 */
# define LVDS_A0A2_CLKA_POWER_MASK	(3 << 8)
# define LVDS_A0A2_CLKA_POWER_DOWN	(0 << 8)
# define LVDS_A0A2_CLKA_POWER_UP	(3 << 8)
/**
 * Controls the A3 data pair, which contains the additional LSBs for 24 bit
 * mode.  Only enabled if LVDS_A0A2_CLKA_POWER_UP also indicates it should be
 * on.
 */
# define LVDS_A3_POWER_MASK		(3 << 6)
# define LVDS_A3_POWER_DOWN		(0 << 6)
# define LVDS_A3_POWER_UP		(3 << 6)
/**
 * Controls the CLKB pair.  This should only be set when LVDS_B0B3_POWER_UP
 * is set.
 */
# define LVDS_CLKB_POWER_MASK		(3 << 4)
# define LVDS_CLKB_POWER_DOWN		(0 << 4)
# define LVDS_CLKB_POWER_UP		(3 << 4)

/**
 * Controls the B0-B3 data pairs.  This must be set to match the DPLL p2
 * setting for whether we are in dual-channel mode.  The B3 pair will
 * additionally only be powered up when LVDS_A3_POWER_UP is set.
 */
# define LVDS_B0B3_POWER_MASK		(3 << 2)
# define LVDS_B0B3_POWER_DOWN		(0 << 2)
# define LVDS_B0B3_POWER_UP		(3 << 2)

#define TV_CTL			0x68000
/** Enables the TV encoder */
# define TV_ENC_ENABLE			(1 << 31)
/** Sources the TV encoder input from pipe B instead of A. */
# define TV_ENC_PIPEB_SELECT		(1 << 30)
/** Outputs composite video (DAC A only) */
# define TV_ENC_OUTPUT_COMPOSITE	(0 << 28)
/** Outputs SVideo video (DAC B/C) */
# define TV_ENC_OUTPUT_SVIDEO		(1 << 28)
/** Outputs Component video (DAC A/B/C) */
# define TV_ENC_OUTPUT_COMPONENT	(2 << 28)
/** Outputs Composite and SVideo (DAC A/B/C) */
# define TV_ENC_OUTPUT_SVIDEO_COMPOSITE	(3 << 28)
# define TV_TRILEVEL_SYNC		(1 << 21)
/** Enables slow sync generation (945GM only) */
# define TV_SLOW_SYNC			(1 << 20)
/** Selects 4x oversampling for 480i and 576p */
# define TV_OVERSAMPLE_4X		(0 << 18)
/** Selects 2x oversampling for 720p and 1080i */
# define TV_OVERSAMPLE_2X		(1 << 18)
/** Selects no oversampling for 1080p */
# define TV_OVERSAMPLE_NONE		(2 << 18)
/** Selects 8x oversampling */
# define TV_OVERSAMPLE_8X		(3 << 18)
/** Selects progressive mode rather than interlaced */
# define TV_PROGRESSIVE			(1 << 17)
/** Sets the colorburst to PAL mode.  Required for non-M PAL modes. */
# define TV_PAL_BURST			(1 << 16)
/** Field for setting delay of Y compared to C */
# define TV_YC_SKEW_MASK		(7 << 12)
/** Enables a fix for 480p/576p standard definition modes on the 915GM only */
# define TV_ENC_SDP_FIX			(1 << 11)
/**
 * Enables a fix for the 915GM only.
 *
 * Not sure what it does.
 */
# define TV_ENC_C0_FIX			(1 << 10)
/** Bits that must be preserved by software */
# define TV_CTL_SAVE			((3 << 8) | (3 << 6))
# define TV_FUSE_STATE_MASK		(3 << 4)
/** Read-only state that reports all features enabled */
# define TV_FUSE_STATE_ENABLED		(0 << 4)
/** Read-only state that reports that Macrovision is disabled in hardware*/
# define TV_FUSE_STATE_NO_MACROVISION	(1 << 4)
/** Read-only state that reports that TV-out is disabled in hardware. */
# define TV_FUSE_STATE_DISABLED		(2 << 4)
/** Normal operation */
# define TV_TEST_MODE_NORMAL		(0 << 0)
/** Encoder test pattern 1 - combo pattern */
# define TV_TEST_MODE_PATTERN_1		(1 << 0)
/** Encoder test pattern 2 - full screen vertical 75% color bars */
# define TV_TEST_MODE_PATTERN_2		(2 << 0)
/** Encoder test pattern 3 - full screen horizontal 75% color bars */
# define TV_TEST_MODE_PATTERN_3		(3 << 0)
/** Encoder test pattern 4 - random noise */
# define TV_TEST_MODE_PATTERN_4		(4 << 0)
/** Encoder test pattern 5 - linear color ramps */
# define TV_TEST_MODE_PATTERN_5		(5 << 0)
/**
 * This test mode forces the DACs to 50% of full output.
 *
 * This is used for load detection in combination with TVDAC_SENSE_MASK
 */
# define TV_TEST_MODE_MONITOR_DETECT	(7 << 0)
# define TV_TEST_MODE_MASK		(7 << 0)
/** @} */

/** @defgroup TV_DAC
 * @{
 */
#define TV_DAC			0x68004
/**
 * Reports that DAC state change logic has reported change (RO).
 *
 * This gets cleared when TV_DAC_STATE_EN is cleared
*/
# define TVDAC_STATE_CHG		(1 << 31)
# define TVDAC_SENSE_MASK		(7 << 28)
/** Reports that DAC A voltage is above the detect threshold */
# define TVDAC_A_SENSE			(1 << 30)
/** Reports that DAC B voltage is above the detect threshold */
# define TVDAC_B_SENSE			(1 << 29)
/** Reports that DAC C voltage is above the detect threshold */
# define TVDAC_C_SENSE			(1 << 28)
/**
 * Enables DAC state detection logic, for load-based TV detection.
 *
 * The PLL of the chosen pipe (in TV_CTL) must be running, and the encoder set
 * to off, for load detection to work.
 */
# define TVDAC_STATE_CHG_EN		(1 << 27)
/** Sets the DAC A sense value to high */
# define TVDAC_A_SENSE_CTL		(1 << 26)
/** Sets the DAC B sense value to high */
# define TVDAC_B_SENSE_CTL		(1 << 25)
/** Sets the DAC C sense value to high */
# define TVDAC_C_SENSE_CTL		(1 << 24)
/** Overrides the ENC_ENABLE and DAC voltage levels */
# define DAC_CTL_OVERRIDE		(1 << 7)
/** Sets the slew rate.  Must be preserved in software */
# define ENC_TVDAC_SLEW_FAST		(1 << 6)
# define DAC_A_1_3_V			(0 << 4)
# define DAC_A_1_1_V			(1 << 4)
# define DAC_A_0_7_V			(2 << 4)
# define DAC_A_OFF			(3 << 4)
# define DAC_B_1_3_V			(0 << 2)
# define DAC_B_1_1_V			(1 << 2)
# define DAC_B_0_7_V			(2 << 2)
# define DAC_B_OFF			(3 << 2)
# define DAC_C_1_3_V			(0 << 0)
# define DAC_C_1_1_V			(1 << 0)
# define DAC_C_0_7_V			(2 << 0)
# define DAC_C_OFF			(3 << 0)
/** @} */

/**
 * CSC coefficients are stored in a floating point format with 9 bits of
 * mantissa and 2 or 3 bits of exponent.  The exponent is represented as 2**-n,
 * where 2-bit exponents are unsigned n, and 3-bit exponents are signed n with
 * -1 (0x3) being the only legal negative value.
 */
#define TV_CSC_Y		0x68010
# define TV_RY_MASK			0x07ff0000
# define TV_RY_SHIFT			16
# define TV_GY_MASK			0x00000fff
# define TV_GY_SHIFT			0

#define TV_CSC_Y2		0x68014
# define TV_BY_MASK			0x07ff0000
# define TV_BY_SHIFT			16
/**
 * Y attenuation for component video.
 *
 * Stored in 1.9 fixed point.
 */
# define TV_AY_MASK			0x000003ff
# define TV_AY_SHIFT			0

#define TV_CSC_U		0x68018
# define TV_RU_MASK			0x07ff0000
# define TV_RU_SHIFT			16
# define TV_GU_MASK			0x000007ff
# define TV_GU_SHIFT			0

#define TV_CSC_U2		0x6801c
# define TV_BU_MASK			0x07ff0000
# define TV_BU_SHIFT			16
/**
 * U attenuation for component video.
 *
 * Stored in 1.9 fixed point.
 */
# define TV_AU_MASK			0x000003ff
# define TV_AU_SHIFT			0

#define TV_CSC_V		0x68020
# define TV_RV_MASK			0x0fff0000
# define TV_RV_SHIFT			16
# define TV_GV_MASK			0x000007ff
# define TV_GV_SHIFT			0

#define TV_CSC_V2		0x68024
# define TV_BV_MASK			0x07ff0000
# define TV_BV_SHIFT			16
/**
 * V attenuation for component video.
 *
 * Stored in 1.9 fixed point.
 */
# define TV_AV_MASK			0x000007ff
# define TV_AV_SHIFT			0

/** @defgroup TV_CSC_KNOBS
 * @{
 */
#define TV_CLR_KNOBS		0x68028
/** 2s-complement brightness adjustment */
# define TV_BRIGHTNESS_MASK		0xff000000
# define TV_BRIGHTNESS_SHIFT		24
/** Contrast adjustment, as a 2.6 unsigned floating point number */
# define TV_CONTRAST_MASK		0x00ff0000
# define TV_CONTRAST_SHIFT		16
/** Saturation adjustment, as a 2.6 unsigned floating point number */
# define TV_SATURATION_MASK		0x0000ff00
# define TV_SATURATION_SHIFT		8
/** Hue adjustment, as an integer phase angle in degrees */
# define TV_HUE_MASK			0x000000ff
# define TV_HUE_SHIFT			0
/** @} */

/** @defgroup TV_CLR_LEVEL
 * @{
 */
#define TV_CLR_LEVEL		0x6802c
/** Controls the DAC level for black */
# define TV_BLACK_LEVEL_MASK		0x01ff0000
# define TV_BLACK_LEVEL_SHIFT		16
/** Controls the DAC level for blanking */
# define TV_BLANK_LEVEL_MASK		0x000001ff
# define TV_BLANK_LEVEL_SHIFT		0
/* @} */

/** @defgroup TV_H_CTL_1
 * @{
 */
#define TV_H_CTL_1		0x68030
/** Number of pixels in the hsync. */
# define TV_HSYNC_END_MASK		0x1fff0000
# define TV_HSYNC_END_SHIFT		16
/** Total number of pixels minus one in the line (display and blanking). */
# define TV_HTOTAL_MASK			0x00001fff
# define TV_HTOTAL_SHIFT		0
/** @} */

/** @defgroup TV_H_CTL_2
 * @{
 */
#define TV_H_CTL_2		0x68034
/** Enables the colorburst (needed for non-component color) */
# define TV_BURST_ENA			(1 << 31)
/** Offset of the colorburst from the start of hsync, in pixels minus one. */
# define TV_HBURST_START_SHIFT		16
# define TV_HBURST_START_MASK		0x1fff0000
/** Length of the colorburst */
# define TV_HBURST_LEN_SHIFT		0
# define TV_HBURST_LEN_MASK		0x0001fff
/** @} */

/** @defgroup TV_H_CTL_3
 * @{
 */
#define TV_H_CTL_3		0x68038
/** End of hblank, measured in pixels minus one from start of hsync */
# define TV_HBLANK_END_SHIFT		16
# define TV_HBLANK_END_MASK		0x1fff0000
/** Start of hblank, measured in pixels minus one from start of hsync */
# define TV_HBLANK_START_SHIFT		0
# define TV_HBLANK_START_MASK		0x0001fff
/** @} */

/** @defgroup TV_V_CTL_1
 * @{
 */
#define TV_V_CTL_1		0x6803c
/** XXX */
# define TV_NBR_END_SHIFT		16
# define TV_NBR_END_MASK		0x07ff0000
/** XXX */
# define TV_VI_END_F1_SHIFT		8
# define TV_VI_END_F1_MASK		0x00003f00
/** XXX */
# define TV_VI_END_F2_SHIFT		0
# define TV_VI_END_F2_MASK		0x0000003f
/** @} */

/** @defgroup TV_V_CTL_2
 * @{
 */
#define TV_V_CTL_2		0x68040
/** Length of vsync, in half lines */
# define TV_VSYNC_LEN_MASK		0x07ff0000
# define TV_VSYNC_LEN_SHIFT		16
/** Offset of the start of vsync in field 1, measured in one less than the
 * number of half lines.
 */
# define TV_VSYNC_START_F1_MASK		0x00007f00
# define TV_VSYNC_START_F1_SHIFT	8
/**
 * Offset of the start of vsync in field 2, measured in one less than the
 * number of half lines.
 */
# define TV_VSYNC_START_F2_MASK		0x0000007f
# define TV_VSYNC_START_F2_SHIFT	0
/** @} */

/** @defgroup TV_V_CTL_3
 * @{
 */
#define TV_V_CTL_3		0x68044
/** Enables generation of the equalization signal */
# define TV_EQUAL_ENA			(1 << 31)
/** Length of vsync, in half lines */
# define TV_VEQ_LEN_MASK		0x007f0000
# define TV_VEQ_LEN_SHIFT		16
/** Offset of the start of equalization in field 1, measured in one less than
 * the number of half lines.
 */
# define TV_VEQ_START_F1_MASK		0x0007f00
# define TV_VEQ_START_F1_SHIFT		8
/**
 * Offset of the start of equalization in field 2, measured in one less than
 * the number of half lines.
 */
# define TV_VEQ_START_F2_MASK		0x000007f
# define TV_VEQ_START_F2_SHIFT		0
/** @} */

/** @defgroup TV_V_CTL_4
 * @{
 */
#define TV_V_CTL_4		0x68048
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F1_MASK	0x003f0000
# define TV_VBURST_START_F1_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F1_MASK		0x000000ff
# define TV_VBURST_END_F1_SHIFT		0
/** @} */

/** @defgroup TV_V_CTL_5
 * @{
 */
#define TV_V_CTL_5		0x6804c
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F2_MASK	0x003f0000
# define TV_VBURST_START_F2_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F2_MASK		0x000000ff
# define TV_VBURST_END_F2_SHIFT		0
/** @} */

/** @defgroup TV_V_CTL_6
 * @{
 */
#define TV_V_CTL_6		0x68050
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F3_MASK	0x003f0000
# define TV_VBURST_START_F3_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F3_MASK		0x000000ff
# define TV_VBURST_END_F3_SHIFT		0
/** @} */

/** @defgroup TV_V_CTL_7
 * @{
 */
#define TV_V_CTL_7		0x68054
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F4_MASK	0x003f0000
# define TV_VBURST_START_F4_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F4_MASK		0x000000ff
# define TV_VBURST_END_F4_SHIFT		0
/** @} */

/** @defgroup TV_SC_CTL_1
 * @{
 */
#define TV_SC_CTL_1		0x68060
/** Turns on the first subcarrier phase generation DDA */
# define TV_SC_DDA1_EN			(1 << 31)
/** Turns on the first subcarrier phase generation DDA */
# define TV_SC_DDA2_EN			(1 << 30)
/** Turns on the first subcarrier phase generation DDA */
# define TV_SC_DDA3_EN			(1 << 29)
/** Sets the subcarrier DDA to reset frequency every other field */
# define TV_SC_RESET_EVERY_2		(0 << 24)
/** Sets the subcarrier DDA to reset frequency every fourth field */
# define TV_SC_RESET_EVERY_4		(1 << 24)
/** Sets the subcarrier DDA to reset frequency every eighth field */
# define TV_SC_RESET_EVERY_8		(2 << 24)
/** Sets the subcarrier DDA to never reset the frequency */
# define TV_SC_RESET_NEVER		(3 << 24)
/** Sets the peak amplitude of the colorburst.*/
# define TV_BURST_LEVEL_MASK		0x00ff0000
# define TV_BURST_LEVEL_SHIFT		16
/** Sets the increment of the first subcarrier phase generation DDA */
# define TV_SCDDA1_INC_MASK		0x00000fff
# define TV_SCDDA1_INC_SHIFT		0
/** @} */

/** @defgroup TV_SC_CTL_2
 * @{
 */
#define TV_SC_CTL_2		0x68064
/** Sets the rollover for the second subcarrier phase generation DDA */
# define TV_SCDDA2_SIZE_MASK		0x7fff0000
# define TV_SCDDA2_SIZE_SHIFT		16
/** Sets the increent of the second subcarrier phase generation DDA */
# define TV_SCDDA2_INC_MASK		0x00007fff
# define TV_SCDDA2_INC_SHIFT		0
/** @} */

/** @defgroup TV_SC_CTL_3
 * @{
 */
#define TV_SC_CTL_3		0x68068
/** Sets the rollover for the third subcarrier phase generation DDA */
# define TV_SCDDA3_SIZE_MASK		0x7fff0000
# define TV_SCDDA3_SIZE_SHIFT		16
/** Sets the increent of the third subcarrier phase generation DDA */
# define TV_SCDDA3_INC_MASK		0x00007fff
# define TV_SCDDA3_INC_SHIFT		0
/** @} */

/** @defgroup TV_WIN_POS
 * @{
 */
#define TV_WIN_POS		0x68070
/** X coordinate of the display from the start of horizontal active */
# define TV_XPOS_MASK			0x1fff0000
# define TV_XPOS_SHIFT			16
/** Y coordinate of the display from the start of vertical active (NBR) */
# define TV_YPOS_MASK			0x00000fff
# define TV_YPOS_SHIFT			0
/** @} */

/** @defgroup TV_WIN_SIZE
 * @{
 */
#define TV_WIN_SIZE		0x68074
/** Horizontal size of the display window, measured in pixels*/
# define TV_XSIZE_MASK			0x1fff0000
# define TV_XSIZE_SHIFT			16
/**
 * Vertical size of the display window, measured in pixels.
 *
 * Must be even for interlaced modes.
 */
# define TV_YSIZE_MASK			0x00000fff
# define TV_YSIZE_SHIFT			0
/** @} */

/** @defgroup TV_FILTER_CTL_1
 * @{
 */
#define TV_FILTER_CTL_1		0x68080
/**
 * Enables automatic scaling calculation.
 *
 * If set, the rest of the registers are ignored, and the calculated values can
 * be read back from the register.
 */
# define TV_AUTO_SCALE			(1 << 31)
/**
 * Disables the vertical filter.
 *
 * This is required on modes more than 1024 pixels wide */
# define TV_V_FILTER_BYPASS		(1 << 29)
/** Enables adaptive vertical filtering */
# define TV_VADAPT			(1 << 28)
# define TV_VADAPT_MODE_MASK		(3 << 26)
/** Selects the least adaptive vertical filtering mode */
# define TV_VADAPT_MODE_LEAST		(0 << 26)
/** Selects the moderately adaptive vertical filtering mode */
# define TV_VADAPT_MODE_MODERATE	(1 << 26)
/** Selects the most adaptive vertical filtering mode */
# define TV_VADAPT_MODE_MOST		(3 << 26)
/**
 * Sets the horizontal scaling factor.
 *
 * This should be the fractional part of the horizontal scaling factor divided
 * by the oversampling rate.  TV_HSCALE should be less than 1, and set to:
 *
 * (src width - 1) / ((oversample * dest width) - 1)
 */
# define TV_HSCALE_FRAC_MASK		0x00003fff
# define TV_HSCALE_FRAC_SHIFT		0
/** @} */

/** @defgroup TV_FILTER_CTL_2
 * @{
 */
#define TV_FILTER_CTL_2		0x68084
/**
 * Sets the integer part of the 3.15 fixed-point vertical scaling factor.
 *
 * TV_VSCALE should be (src height - 1) / ((interlace * dest height) - 1)
 */
# define TV_VSCALE_INT_MASK		0x00038000
# define TV_VSCALE_INT_SHIFT		15
/**
 * Sets the fractional part of the 3.15 fixed-point vertical scaling factor.
 *
 * \sa TV_VSCALE_INT_MASK
 */
# define TV_VSCALE_FRAC_MASK		0x00007fff
# define TV_VSCALE_FRAC_SHIFT		0
/** @} */

/** @defgroup TV_FILTER_CTL_3
 * @{
 */
#define TV_FILTER_CTL_3		0x68088
/**
 * Sets the integer part of the 3.15 fixed-point vertical scaling factor.
 *
 * TV_VSCALE should be (src height - 1) / (1/4 * (dest height - 1))
 *
 * For progressive modes, TV_VSCALE_IP_INT should be set to zeroes.
 */
# define TV_VSCALE_IP_INT_MASK		0x00038000
# define TV_VSCALE_IP_INT_SHIFT		15
/**
 * Sets the fractional part of the 3.15 fixed-point vertical scaling factor.
 *
 * For progressive modes, TV_VSCALE_IP_INT should be set to zeroes.
 *
 * \sa TV_VSCALE_IP_INT_MASK
 */
# define TV_VSCALE_IP_FRAC_MASK		0x00007fff
# define TV_VSCALE_IP_FRAC_SHIFT		0
/** @} */

/** @defgroup TV_CC_CONTROL
 * @{
 */
#define TV_CC_CONTROL		0x68090
# define TV_CC_ENABLE			(1 << 31)
/**
 * Specifies which field to send the CC data in.
 *
 * CC data is usually sent in field 0.
 */
# define TV_CC_FID_MASK			(1 << 27)
# define TV_CC_FID_SHIFT		27
/** Sets the horizontal position of the CC data.  Usually 135. */
# define TV_CC_HOFF_MASK		0x03ff0000
# define TV_CC_HOFF_SHIFT		16
/** Sets the vertical position of the CC data.  Usually 21 */
# define TV_CC_LINE_MASK		0x0000003f
# define TV_CC_LINE_SHIFT		0
/** @} */

/** @defgroup TV_CC_DATA
 * @{
 */
#define TV_CC_DATA		0x68094
# define TV_CC_RDY			(1 << 31)
/** Second word of CC data to be transmitted. */
# define TV_CC_DATA_2_MASK		0x007f0000
# define TV_CC_DATA_2_SHIFT		16
/** First word of CC data to be transmitted. */
# define TV_CC_DATA_1_MASK		0x0000007f
# define TV_CC_DATA_1_SHIFT		0
/** @}
 */

/** @{ */
#define TV_H_LUMA_0		0x68100
#define TV_H_LUMA_59		0x681ec
#define TV_H_CHROMA_0		0x68200
#define TV_H_CHROMA_59		0x682ec
#define TV_V_LUMA_0		0x68300
#define TV_V_LUMA_42		0x683a8
#define TV_V_CHROMA_0		0x68400
#define TV_V_CHROMA_42		0x684a8

#define PIPEACONF 0x70008
#define PIPEACONF_ENABLE	(1<<31)
#define PIPEACONF_DISABLE	0
#define PIPEACONF_DOUBLE_WIDE	(1<<30)
#define I965_PIPECONF_ACTIVE	(1<<30)
#define PIPEACONF_SINGLE_WIDE	0
#define PIPEACONF_PIPE_UNLOCKED 0
#define PIPEACONF_PIPE_LOCKED	(1<<25)
#define PIPEACONF_PALETTE	0
#define PIPEACONF_GAMMA		(1<<24)
#define PIPECONF_FORCE_BORDER	(1<<25)
#define PIPECONF_PROGRESSIVE	(0 << 21)
#define PIPECONF_INTERLACE_W_FIELD_INDICATION	(6 << 21)
#define PIPECONF_INTERLACE_FIELD_0_ONLY		(7 << 21)

#define PIPEBCONF 0x71008
#define PIPEBCONF_ENABLE	(1<<31)
#define PIPEBCONF_DISABLE	0
#define PIPEBCONF_DOUBLE_WIDE	(1<<30)
#define PIPEBCONF_DISABLE	0
#define PIPEBCONF_GAMMA		(1<<24)
#define PIPEBCONF_PALETTE	0

#define PIPEBGCMAXRED		0x71010
#define PIPEBGCMAXGREEN		0x71014
#define PIPEBGCMAXBLUE		0x71018
#define PIPEBSTAT		0x71024
#define PIPEBFRAMEHIGH		0x71040
#define PIPEBFRAMEPIXEL		0x71044

#define DSPACNTR		0x70180
#define DSPBCNTR		0x71180
#define DISPLAY_PLANE_ENABLE			(1<<31)
#define DISPLAY_PLANE_DISABLE			0
#define DISPPLANE_GAMMA_ENABLE			(1<<30)
#define DISPPLANE_GAMMA_DISABLE			0
#define DISPPLANE_PIXFORMAT_MASK		(0xf<<26)
#define DISPPLANE_8BPP				(0x2<<26)
#define DISPPLANE_15_16BPP			(0x4<<26)
#define DISPPLANE_16BPP				(0x5<<26)
#define DISPPLANE_32BPP_NO_ALPHA		(0x6<<26)
#define DISPPLANE_32BPP				(0x7<<26)
#define DISPPLANE_STEREO_ENABLE			(1<<25)
#define DISPPLANE_STEREO_DISABLE		0
#define DISPPLANE_SEL_PIPE_MASK			(1<<24)
#define DISPPLANE_SEL_PIPE_A			0
#define DISPPLANE_SEL_PIPE_B			(1<<24)
#define DISPPLANE_SRC_KEY_ENABLE		(1<<22)
#define DISPPLANE_SRC_KEY_DISABLE		0
#define DISPPLANE_LINE_DOUBLE			(1<<20)
#define DISPPLANE_NO_LINE_DOUBLE		0
#define DISPPLANE_STEREO_POLARITY_FIRST		0
#define DISPPLANE_STEREO_POLARITY_SECOND	(1<<18)
/* plane B only */
#define DISPPLANE_ALPHA_TRANS_ENABLE		(1<<15)
#define DISPPLANE_ALPHA_TRANS_DISABLE		0
#define DISPPLANE_SPRITE_ABOVE_DISPLAYA		0
#define DISPPLANE_SPRITE_ABOVE_OVERLAY		(1)

#define DSPABASE		0x70184
#define DSPASTRIDE		0x70188

#define DSPBBASE		0x71184
#define DSPBADDR		DSPBBASE
#define DSPBSTRIDE		0x71188

#define DSPAKEYVAL		0x70194
#define DSPAKEYMASK		0x70198

#define DSPAPOS			0x7018C /* reserved */
#define DSPASIZE		0x70190
#define DSPBPOS			0x7118C
#define DSPBSIZE		0x71190

#define DSPASURF		0x7019C
#define DSPATILEOFF		0x701A4

#define DSPBSURF		0x7119C
#define DSPBTILEOFF		0x711A4

#define VGACNTRL		0x71400
# define VGA_DISP_DISABLE			(1 << 31)
# define VGA_2X_MODE				(1 << 30)
# define VGA_PIPE_B_SELECT			(1 << 29)

/*
 * Some BIOS scratch area registers.  The 845 (and 830?) store the amount
 * of video memory available to the BIOS in SWF1.
 */

#define SWF0			0x71410
#define SWF1			0x71414
#define SWF2			0x71418
#define SWF3			0x7141c
#define SWF4			0x71420
#define SWF5			0x71424
#define SWF6			0x71428

#define SWF10			0x70410
#define SWF30			0x72414
#define SWF31			0x72418
#define SWF32			0x7241c

/*
 * Overlay registers.  These are overlay registers accessed via MMIO.
 * Those loaded via the overlay register page are defined in i830_video.c.
 */
#define OVADD			0x30000

#define DOVSTA			0x30008
#define OC_BUF			(0x3<<20)

#define OGAMC5			0x30010
#define OGAMC4			0x30014
#define OGAMC3			0x30018
#define OGAMC2			0x3001c
#define OGAMC1			0x30020
#define OGAMC0			0x30024

/*
 * Palette registers
 */
#define PALETTE_A		0x0a000
#define PALETTE_B		0x0a800

#define IS_I830(dev) ((dev)->pci_device == 0x3577)
#define IS_845G(dev) ((dev)->pci_device == 0x2562)
#define IS_I85X(dev) ((dev)->pci_device == 0x3582)
#define IS_I855(dev) ((dev)->pci_device == 0x3582)
#define IS_I865G(dev) ((dev)->pci_device == 0x2572)

#define IS_I915G(dev) ((dev)->pci_device == 0x2582 || (dev)->pci_device == 0x258a)
#define IS_I915GM(dev) ((dev)->pci_device == 0x2592)
#define IS_I945G(dev) ((dev)->pci_device == 0x2772)
#define IS_I945GM(dev) ((dev)->pci_device == 0x27A2)

#define IS_I965G(dev) ((dev)->pci_device == 0x2972 || \
		       (dev)->pci_device == 0x2982 || \
		       (dev)->pci_device == 0x2992 || \
		       (dev)->pci_device == 0x29A2 || \
		       (dev)->pci_device == 0x2A02 || \
		       (dev)->pci_device == 0x2A12 || \
		       (dev)->pci_device == 0x2A42)

#define IS_I965GM(dev) ((dev)->pci_device == 0x2A02)

#define IS_IGD_GM(dev) ((dev)->pci_device == 0x2A42)

#define IS_G33(dev)    ((dev)->pci_device == 0x29C2 ||	\
			(dev)->pci_device == 0x29B2 ||	\
			(dev)->pci_device == 0x29D2)

#define IS_I9XX(dev) (IS_I915G(dev) || IS_I915GM(dev) || IS_I945G(dev) || \
		      IS_I945GM(dev) || IS_I965G(dev) || IS_G33(dev))

#define IS_MOBILE(dev) (IS_I830(dev) || IS_I85X(dev) || IS_I915GM(dev) || \
			IS_I945GM(dev) || IS_I965GM(dev) || IS_IGD_GM(dev))

#define PRIMARY_RINGBUFFER_SIZE         (128*1024)

#endif

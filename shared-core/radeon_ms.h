/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Dave Airlie
 * Copyright 2007 Alex Deucher
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * Authors:
 *    Jérôme Glisse <glisse@freedesktop.org>
 */
#ifndef __RADEON_MS_H__
#define __RADEON_MS_H__

#include "radeon_ms_drv.h"
#include "radeon_ms_reg.h"
#include "radeon_ms_drm.h"

#define DRIVER_AUTHOR      "Jerome Glisse, Dave Airlie,  Gareth Hughes, "\
			   "Keith Whitwell, others."
#define DRIVER_NAME        "radeon_ms"
#define DRIVER_DESC        "radeon kernel modesetting"
#define DRIVER_DATE        "20071108"
#define DRIVER_MAJOR        1
#define DRIVER_MINOR        0
#define DRIVER_PATCHLEVEL   0

#define RADEON_PAGE_SIZE        4096
#define RADEON_MAX_CONNECTORS   8
#define RADEON_MAX_OUTPUTS      8

enum radeon_bus_type {
	RADEON_PCI = 0x10000,
	RADEON_AGP = 0x20000,
	RADEON_PCIE = 0x30000,
};

enum radeon_family {
	CHIP_R100,
	CHIP_RV100,
	CHIP_RS100,
	CHIP_RV200,
	CHIP_RS200,
	CHIP_R200,
	CHIP_RV250,
	CHIP_RS300,
	CHIP_RV280,
	CHIP_R300,
	CHIP_R350,
	CHIP_R360,
	CHIP_RV350,
	CHIP_RV370,
	CHIP_RV380,
	CHIP_RS400,
	CHIP_RV410,
	CHIP_R420,
	CHIP_R430,
	CHIP_R480,
	CHIP_LAST,
};

enum radeon_monitor_type {
	MT_UNKNOWN = -1,
	MT_NONE    = 0,
	MT_CRT     = 1,
	MT_LCD     = 2,
	MT_DFP     = 3,
	MT_CTV     = 4,
	MT_STV     = 5
};

enum radeon_connector_type {
	CONNECTOR_NONE,
	CONNECTOR_PROPRIETARY,
	CONNECTOR_VGA,
	CONNECTOR_DVI_I,
	CONNECTOR_DVI_D,
	CONNECTOR_CTV,
	CONNECTOR_STV,
	CONNECTOR_UNSUPPORTED
};

enum radeon_output_type {
	OUTPUT_NONE,
	OUTPUT_DAC1,
	OUTPUT_DAC2,
	OUTPUT_TMDS,
	OUTPUT_LVDS
};

struct radeon_state;

struct radeon_ms_crtc {
	int             crtc;
	uint16_t	lut_r[256];
	uint16_t	lut_g[256];
	uint16_t	lut_b[256];
};

struct radeon_ms_i2c {
	struct drm_device           *drm_dev;
	uint32_t    	            reg;
	struct i2c_adapter          adapter;
	struct i2c_algo_bit_data    algo;
};

struct radeon_ms_connector {
	struct radeon_ms_i2c    *i2c;
	struct edid             *edid;
	struct drm_output       *output;
	int                     type;
	int                     monitor_type;
	int                     crtc;
	uint32_t    	        i2c_reg;
	char                    outputs[RADEON_MAX_OUTPUTS];
	char                    name[32];
};

struct radeon_ms_output {
	int                         type;
	struct drm_device           *dev;
	struct radeon_ms_connector  *connector;
	int (*initialize)(struct radeon_ms_output *output);
	enum drm_output_status (*detect)(struct radeon_ms_output *output);
	void (*dpms)(struct radeon_ms_output *output, int mode);
	int (*get_modes)(struct radeon_ms_output *output);
	bool (*mode_fixup)(struct radeon_ms_output *output,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode);
	int (*mode_set)(struct radeon_ms_output *output,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode);
	void (*restore)(struct radeon_ms_output *output,
			struct radeon_state *state);
	void (*save)(struct radeon_ms_output *output,
			struct radeon_state *state);
};

struct radeon_ms_properties {
	uint16_t                    subvendor;
	uint16_t                    subdevice;
	int16_t                     pll_reference_freq;
	int32_t                     pll_min_pll_freq;
	int32_t                     pll_max_pll_freq;
	char                        pll_use_bios;
	char                        pll_dummy_reads;
	char                        pll_delay;
	char                        pll_r300_errata;
	struct radeon_ms_output     *outputs[RADEON_MAX_OUTPUTS];
	struct radeon_ms_connector  *connectors[RADEON_MAX_CONNECTORS];
};

struct radeon_state {
	/* memory */
	uint32_t        config_aper_0_base;
	uint32_t        config_aper_1_base;
	uint32_t        config_aper_size;
	uint32_t        mc_fb_location;
	uint32_t        display_base_addr;
	/* irq */
	uint32_t	gen_int_cntl;
	/* pci */
	uint32_t        aic_ctrl;
	uint32_t        aic_pt_base;
	uint32_t        aic_pt_base_lo;
	uint32_t        aic_pt_base_hi;
	uint32_t        aic_lo_addr;
	uint32_t        aic_hi_addr;
	/* agp */
	uint32_t        agp_cntl;
	uint32_t        agp_command;
	uint32_t        agp_base;
	uint32_t        agp_base_2;
	uint32_t        bus_cntl;
	uint32_t        mc_agp_location;
	/* cp */
	uint32_t        cp_rb_cntl;
	uint32_t        cp_rb_base;
	uint32_t        cp_rb_rptr_addr;
	uint32_t        cp_rb_wptr;
	uint32_t        cp_rb_wptr_delay;
	uint32_t        scratch_umsk;
	uint32_t        scratch_addr;
	/* pcie */
	uint32_t        pcie_tx_gart_cntl;
	uint32_t        pcie_tx_gart_discard_rd_addr_lo;
	uint32_t        pcie_tx_gart_discard_rd_addr_hi;
	uint32_t        pcie_tx_gart_base;
	uint32_t        pcie_tx_gart_start_lo;
	uint32_t        pcie_tx_gart_start_hi;
	uint32_t        pcie_tx_gart_end_lo;
	uint32_t        pcie_tx_gart_end_hi;
	/* surface */
	uint32_t        surface_cntl;
	uint32_t        surface0_info;
	uint32_t        surface0_lower_bound;
	uint32_t        surface0_upper_bound;
	uint32_t        surface1_info;
	uint32_t        surface1_lower_bound;
	uint32_t        surface1_upper_bound;
	uint32_t        surface2_info;
	uint32_t        surface2_lower_bound;
	uint32_t        surface2_upper_bound;
	uint32_t        surface3_info;
	uint32_t        surface3_lower_bound;
	uint32_t        surface3_upper_bound;
	uint32_t        surface4_info;
	uint32_t        surface4_lower_bound;
	uint32_t        surface4_upper_bound;
	uint32_t        surface5_info;
	uint32_t        surface5_lower_bound;
	uint32_t        surface5_upper_bound;
	uint32_t        surface6_info;
	uint32_t        surface6_lower_bound;
	uint32_t        surface6_upper_bound;
	uint32_t        surface7_info;
	uint32_t        surface7_lower_bound;
	uint32_t        surface7_upper_bound;
	/* crtc */
	uint32_t        crtc_gen_cntl;
	uint32_t        crtc_ext_cntl;
	uint32_t        crtc_h_total_disp;
	uint32_t        crtc_h_sync_strt_wid;
	uint32_t        crtc_v_total_disp;
	uint32_t        crtc_v_sync_strt_wid;
	uint32_t        crtc_offset;
	uint32_t        crtc_offset_cntl;
	uint32_t        crtc_pitch;
	uint32_t        crtc_more_cntl;
	uint32_t        crtc_tile_x0_y0;
	uint32_t        fp_h_sync_strt_wid;
	uint32_t        fp_v_sync_strt_wid;
	uint32_t        fp_crtc_h_total_disp;
	uint32_t        fp_crtc_v_total_disp;
	/* pll */
	uint32_t        clock_cntl_index;
	uint32_t        ppll_cntl;
	uint32_t        ppll_ref_div;
	uint32_t        ppll_div_0;
	uint32_t        ppll_div_1;
	uint32_t        ppll_div_2;
	uint32_t        ppll_div_3;
	uint32_t        vclk_ecp_cntl;
	uint32_t        htotal_cntl;
	/* dac */
	uint32_t        dac_cntl;
	uint32_t        dac_cntl2;
	uint32_t        dac_ext_cntl;
	uint32_t        disp_misc_cntl;
	uint32_t        dac_macro_cntl;
	uint32_t        disp_pwr_man;
	uint32_t        disp_merge_cntl;
	uint32_t        disp_output_cntl;
	uint32_t        disp2_merge_cntl;
	uint32_t        dac_embedded_sync_cntl;
	uint32_t        dac_broad_pulse;
	uint32_t        dac_skew_clks;
	uint32_t        dac_incr;
	uint32_t        dac_neg_sync_level;
	uint32_t        dac_pos_sync_level;
	uint32_t        dac_blank_level;
	uint32_t        dac_sync_equalization;
	uint32_t        tv_dac_cntl;
	uint32_t        tv_master_cntl;
};

struct drm_radeon_private {
	/* driver family specific functions */
	int (*bus_finish)(struct drm_device *dev);
	int (*bus_init)(struct drm_device *dev);
	void (*bus_restore)(struct drm_device *dev, struct radeon_state *state);
	void (*bus_save)(struct drm_device *dev, struct radeon_state *state);
	struct drm_ttm_backend *(*create_ttm)(struct drm_device *dev);
	void (*irq_emit)(struct drm_device *dev);
	void (*flush_cache)(struct drm_device *dev);
	/* bus informations */
	void                        *bus;
	uint32_t                    bus_type;
	/* cp */
	uint32_t                    ring_buffer_size;
	uint32_t                    ring_rptr;
	uint32_t                    ring_wptr;
	uint32_t                    ring_mask;
	int                         ring_free;
	uint32_t                    ring_tail_mask;
	uint32_t                    write_back_area_size;
	struct drm_buffer_object    *ring_buffer_object;
	struct drm_bo_kmap_obj      ring_buffer_map;
	uint32_t                    *ring_buffer;
	uint32_t                    *write_back_area;
	const uint32_t              *microcode;
	/* card family */
	uint32_t                    usec_timeout;
	uint32_t                    family;
	struct radeon_ms_properties *properties;
	struct radeon_ms_output     *outputs[RADEON_MAX_OUTPUTS];
	struct radeon_ms_connector  *connectors[RADEON_MAX_CONNECTORS];
	/* drm map (MMIO, FB) */
	struct drm_map              mmio;
	struct drm_map              vram;
	/* gpu address space */
	uint32_t                    gpu_vram_size;
	uint32_t                    gpu_vram_start;
	uint32_t                    gpu_vram_end;
	uint32_t                    gpu_gart_size;
	uint32_t                    gpu_gart_start;
	uint32_t                    gpu_gart_end;
	/* state of the card when module was loaded */
	struct radeon_state         load_state;
	/* state the driver wants */
	struct radeon_state         driver_state;
	/* last emitted fence */
	uint32_t                    fence_id_last;
	uint32_t                    fence_reg;
	/* when doing gpu stop we save here current state */
	uint32_t                    crtc_ext_cntl;
	uint32_t                    crtc_gen_cntl;
	uint32_t                    crtc2_gen_cntl;
	uint32_t                    ov0_scale_cntl;
	/* bool & type on the hw */
	uint8_t                     crtc1_dpms;
	uint8_t                     crtc2_dpms;
	uint8_t                     restore_state;
	uint8_t                     cp_ready;
	uint8_t                     bus_ready;
	uint8_t                     write_back;
};


/* radeon_ms_bo.c */
int radeon_ms_bo_get_gpu_addr(struct drm_device *dev,
			      struct drm_bo_mem_reg *mem,
			      uint32_t *gpu_addr);
int radeon_ms_bo_move(struct drm_buffer_object * bo, int evict,
		      int no_wait, struct drm_bo_mem_reg * new_mem);
struct drm_ttm_backend *radeon_ms_create_ttm_backend(struct drm_device * dev);
uint32_t radeon_ms_evict_mask(struct drm_buffer_object *bo);
int radeon_ms_init_mem_type(struct drm_device * dev, uint32_t type,
			    struct drm_mem_type_manager * man);
int radeon_ms_invalidate_caches(struct drm_device * dev, uint64_t flags);
void radeon_ms_ttm_flush(struct drm_ttm *ttm);

/* radeon_ms_bus.c */
int radeon_ms_agp_finish(struct drm_device *dev);
int radeon_ms_agp_init(struct drm_device *dev);
void radeon_ms_agp_restore(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_agp_save(struct drm_device *dev, struct radeon_state *state);
struct drm_ttm_backend *radeon_ms_pcie_create_ttm(struct drm_device *dev);
int radeon_ms_pcie_finish(struct drm_device *dev);
int radeon_ms_pcie_init(struct drm_device *dev);
void radeon_ms_pcie_restore(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_pcie_save(struct drm_device *dev, struct radeon_state *state);

/* radeon_ms_compat.c */
long radeon_ms_compat_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg);

/* radeon_ms_cp.c */
int radeon_ms_cp_finish(struct drm_device *dev);
int radeon_ms_cp_init(struct drm_device *dev);
void radeon_ms_cp_restore(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_cp_save(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_cp_stop(struct drm_device *dev);
int radeon_ms_cp_wait(struct drm_device *dev, int n);
int radeon_ms_ring_emit(struct drm_device *dev, uint32_t *cmd, uint32_t count);

/* radeon_ms_crtc.c */
int radeon_ms_crtc_create(struct drm_device *dev, int crtc);
void radeon_ms_crtc1_restore(struct drm_device *dev,
			     struct radeon_state *state);
void radeon_ms_crtc1_save(struct drm_device *dev, struct radeon_state *state);

/* radeon_ms_dac.c */
int radeon_ms_dac1_initialize(struct radeon_ms_output *output);
enum drm_output_status radeon_ms_dac1_detect(struct radeon_ms_output *output);
void radeon_ms_dac1_dpms(struct radeon_ms_output *output, int mode);
int radeon_ms_dac1_get_modes(struct radeon_ms_output *output);
bool radeon_ms_dac1_mode_fixup(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode);
int radeon_ms_dac1_mode_set(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode);
void radeon_ms_dac1_restore(struct radeon_ms_output *output,
		struct radeon_state *state);
void radeon_ms_dac1_save(struct radeon_ms_output *output,
		struct radeon_state *state);
int radeon_ms_dac2_initialize(struct radeon_ms_output *output);
enum drm_output_status radeon_ms_dac2_detect(struct radeon_ms_output *output);
void radeon_ms_dac2_dpms(struct radeon_ms_output *output, int mode);
int radeon_ms_dac2_get_modes(struct radeon_ms_output *output);
bool radeon_ms_dac2_mode_fixup(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode);
int radeon_ms_dac2_mode_set(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode);
void radeon_ms_dac2_restore(struct radeon_ms_output *output,
		struct radeon_state *state);
void radeon_ms_dac2_save(struct radeon_ms_output *output,
		struct radeon_state *state);

/* radeon_ms_drm.c */
int radeon_ms_driver_dma_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
void radeon_ms_driver_lastclose(struct drm_device * dev);
int radeon_ms_driver_load(struct drm_device *dev, unsigned long flags);
int radeon_ms_driver_open(struct drm_device * dev, struct drm_file *file_priv);
int radeon_ms_driver_unload(struct drm_device *dev);

/* radeon_ms_exec.c */
int radeon_ms_execbuffer(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);

/* radeon_ms_family.c */
int radeon_ms_family_init(struct drm_device *dev);

/* radeon_ms_fence.c */
int radeon_ms_fence_emit_sequence(struct drm_device *dev, uint32_t class,
			 	  uint32_t flags, uint32_t *sequence,
				  uint32_t *native_type);
void radeon_ms_fence_handler(struct drm_device * dev);
int radeon_ms_fence_has_irq(struct drm_device *dev, uint32_t class,
			    uint32_t flags);
int radeon_ms_fence_types(struct drm_buffer_object *bo,
			  uint32_t * class, uint32_t * type);
void radeon_ms_poke_flush(struct drm_device * dev, uint32_t class);

/* radeon_ms_fb.c */
int radeonfb_probe(struct drm_device *dev, struct drm_crtc *crtc);
int radeonfb_remove(struct drm_device *dev, struct drm_crtc *crtc);

/* radeon_ms_gpu.c */
int radeon_ms_gpu_initialize(struct drm_device *dev);
void radeon_ms_gpu_dpms(struct drm_device *dev);
void radeon_ms_gpu_flush(struct drm_device *dev);
void radeon_ms_gpu_restore(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_gpu_save(struct drm_device *dev, struct radeon_state *state);
int radeon_ms_wait_for_idle(struct drm_device *dev);

/* radeon_ms_i2c.c */
void radeon_ms_i2c_destroy(struct radeon_ms_i2c *i2c);
struct radeon_ms_i2c *radeon_ms_i2c_create(struct drm_device *dev,
					   const uint32_t reg,
					   const char *name);

/* radeon_ms_irq.c */
void radeon_ms_irq_emit(struct drm_device *dev);
irqreturn_t radeon_ms_irq_handler(DRM_IRQ_ARGS);
void radeon_ms_irq_preinstall(struct drm_device * dev);
void radeon_ms_irq_postinstall(struct drm_device * dev);
int radeon_ms_irq_init(struct drm_device *dev);
void radeon_ms_irq_restore(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_irq_save(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_irq_uninstall(struct drm_device * dev);

/* radeon_ms_output.c */
void radeon_ms_connectors_destroy(struct drm_device *dev);
int radeon_ms_connectors_from_properties(struct drm_device *dev);
void radeon_ms_outputs_destroy(struct drm_device *dev);
int radeon_ms_outputs_from_properties(struct drm_device *dev);
void radeon_ms_outputs_restore(struct drm_device *dev,
		struct radeon_state *state);
void radeon_ms_outputs_save(struct drm_device *dev, struct radeon_state *state);

/* radeon_ms_state.c */
void radeon_ms_state_save(struct drm_device *dev, struct radeon_state *state);
void radeon_ms_state_restore(struct drm_device *dev,
			     struct radeon_state *state);


/* packect stuff **************************************************************/
#define RADEON_CP_PACKET0                               0x00000000
#define CP_PACKET0(reg, n)						\
	(RADEON_CP_PACKET0 | ((n) << 16) | ((reg) >> 2))
#define CP_PACKET3_CNTL_BITBLT_MULTI                    0xC0009B00
#    define GMC_SRC_PITCH_OFFSET_CNTL                       (1    <<  0)
#    define GMC_DST_PITCH_OFFSET_CNTL                       (1    <<  1)
#    define GMC_BRUSH_NONE                                  (15   <<  4)
#    define GMC_SRC_DATATYPE_COLOR                          (3    << 12)
#    define ROP3_S                                          0x00cc0000
#    define DP_SRC_SOURCE_MEMORY                            (2    << 24)
#    define GMC_CLR_CMP_CNTL_DIS                            (1    << 28)
#    define GMC_WR_MSK_DIS                                  (1    << 30)

/* helper macro & functions ***************************************************/
#define REG_S(rn, bn, v)    (((v) << rn##__##bn##__SHIFT) & rn##__##bn##__MASK)
#define REG_G(rn, bn, v)    (((v) & rn##__##bn##__MASK) >> rn##__##bn##__SHIFT)
#define MMIO_R(rid)         mmio_read(dev_priv, rid)
#define MMIO_W(rid, v)      mmio_write(dev_priv, rid, v)
#define PCIE_R(rid)         pcie_read(dev_priv, rid)
#define PCIE_W(rid, v)      pcie_write(dev_priv, rid, v)
#define PPLL_R(rid)         pll_read(dev_priv, rid)
#define PPLL_W(rid, v)      pll_write(dev_priv, rid, v)

static __inline__ uint32_t mmio_read(struct drm_radeon_private *dev_priv,
				     uint32_t offset)
{
	return DRM_READ32(&dev_priv->mmio, offset);
}


static __inline__ void mmio_write(struct drm_radeon_private *dev_priv,
				  uint32_t offset, uint32_t v)
{
	DRM_WRITE32(&dev_priv->mmio, offset, v);
}

static __inline__ uint32_t pcie_read(struct drm_radeon_private *dev_priv,
				     uint32_t offset)
{
	MMIO_W(PCIE_INDEX, REG_S(PCIE_INDEX, PCIE_INDEX, offset));
	return MMIO_R(PCIE_DATA);
}

static __inline__ void pcie_write(struct drm_radeon_private *dev_priv,
				  uint32_t offset, uint32_t v)
{
	MMIO_W(PCIE_INDEX, REG_S(PCIE_INDEX, PCIE_INDEX, offset));
	MMIO_W(PCIE_DATA, v);
}

static __inline__ void pll_index_errata(struct drm_radeon_private *dev_priv)
{
	uint32_t tmp, save;

	/* This workaround is necessary on rv200 and RS200 or PLL
	 * reads may return garbage (among others...)
	 */
	if (dev_priv->properties->pll_dummy_reads) {
		tmp = MMIO_R(CLOCK_CNTL_DATA);
		tmp = MMIO_R(CRTC_GEN_CNTL);
	}
	/* This function is required to workaround a hardware bug in some (all?)
	 * revisions of the R300.  This workaround should be called after every
	 * CLOCK_CNTL_INDEX register access.  If not, register reads afterward
	 * may not be correct.
	 */
	if (dev_priv->properties->pll_r300_errata) {
		tmp = save = MMIO_R(CLOCK_CNTL_INDEX);
		tmp = tmp & ~CLOCK_CNTL_INDEX__PLL_ADDR__MASK;
		tmp = tmp & ~CLOCK_CNTL_INDEX__PLL_WR_EN;
		MMIO_W(CLOCK_CNTL_INDEX, tmp);
		tmp = MMIO_R(CLOCK_CNTL_DATA);
		MMIO_W(CLOCK_CNTL_INDEX, save);
	}
}

static __inline__ void pll_data_errata(struct drm_radeon_private *dev_priv)
{
	/* This workarounds is necessary on RV100, RS100 and RS200 chips
	 * or the chip could hang on a subsequent access
	 */
	if (dev_priv->properties->pll_delay) {
		/* we can't deal with posted writes here ... */
		udelay(5000);
	}
}

static __inline__ uint32_t pll_read(struct drm_radeon_private *dev_priv,
				    uint32_t offset)
{
	uint32_t clock_cntl_index = dev_priv->driver_state.clock_cntl_index;
	uint32_t data;

	clock_cntl_index &= ~CLOCK_CNTL_INDEX__PLL_ADDR__MASK;
	clock_cntl_index |= REG_S(CLOCK_CNTL_INDEX, PLL_ADDR, offset);
	MMIO_W(CLOCK_CNTL_INDEX, clock_cntl_index);
	pll_index_errata(dev_priv);
	data = MMIO_R(CLOCK_CNTL_DATA);
	pll_data_errata(dev_priv);
	return data;
}

static __inline__ void pll_write(struct drm_radeon_private *dev_priv,
				 uint32_t offset, uint32_t value)
{
	uint32_t clock_cntl_index = dev_priv->driver_state.clock_cntl_index;

	clock_cntl_index &= ~CLOCK_CNTL_INDEX__PLL_ADDR__MASK;
	clock_cntl_index |= REG_S(CLOCK_CNTL_INDEX, PLL_ADDR, offset);
	clock_cntl_index |= CLOCK_CNTL_INDEX__PLL_WR_EN;
	MMIO_W(CLOCK_CNTL_INDEX, clock_cntl_index);
	pll_index_errata(dev_priv);
	MMIO_W(CLOCK_CNTL_DATA, value);
	pll_data_errata(dev_priv);
}

#endif

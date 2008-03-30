/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Alex Deucher
 * Copyright 2007 Dave Airlie
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
/*
 * Authors:
 *    Dave Airlie <airlied@linux.ie>
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "drmP.h"
#include "drm.h"
#include "radeon_ms.h"

struct radeon_pcie {
	uint32_t                    gart_table_size;
	struct drm_buffer_object    *gart_table_object;
	volatile uint32_t           *gart_table;
	struct drm_device           *dev;
	unsigned long               page_last;
};

struct radeon_pcie_gart {
	struct drm_ttm_backend  backend;
	struct radeon_pcie      *pcie;
	unsigned long           page_first;
	struct page             **pages;
	struct page             *dummy_read_page;
	unsigned long           num_pages;
	int                     populated;
	int                     bound;
};

static int pcie_ttm_bind(struct drm_ttm_backend *backend,
			 struct drm_bo_mem_reg *bo_mem);
static void pcie_ttm_clear(struct drm_ttm_backend *backend);
static void pcie_ttm_destroy(struct drm_ttm_backend *backend);
static int pcie_ttm_needs_ub_cache_adjust(struct drm_ttm_backend *backend);
static int pcie_ttm_populate(struct drm_ttm_backend *backend,
			     unsigned long num_pages, struct page **pages,
			     struct page *dummy_read_page);
static int pcie_ttm_unbind(struct drm_ttm_backend *backend);

static struct drm_ttm_backend_func radeon_pcie_gart_ttm_backend = 
{
	.needs_ub_cache_adjust = pcie_ttm_needs_ub_cache_adjust,
	.populate = pcie_ttm_populate,
	.clear = pcie_ttm_clear,
	.bind = pcie_ttm_bind,
	.unbind = pcie_ttm_unbind,
	.destroy =  pcie_ttm_destroy,
};

static void pcie_gart_flush(struct radeon_pcie *pcie)
{
	struct drm_device *dev;
	struct drm_radeon_private *dev_priv;
	uint32_t flush;

	if (pcie == NULL) {
		return;
	}
	dev = pcie->dev;
	dev_priv = dev->dev_private;
	flush = dev_priv->driver_state.pcie_tx_gart_cntl;
	flush |= PCIE_TX_GART_CNTL__GART_INVALIDATE_TLB;
	PCIE_W(PCIE_TX_GART_CNTL, flush);
	PCIE_W(PCIE_TX_GART_CNTL, dev_priv->driver_state.pcie_tx_gart_cntl);
}

static __inline__ uint32_t pcie_gart_get_page_base(struct radeon_pcie *pcie,
						   unsigned long page)
{
	if (pcie == NULL || pcie->gart_table == NULL) {
		return 0;
	}
	return ((pcie->gart_table[page] & (~0xC)) << 8);
}

static __inline__ void pcie_gart_set_page_base(struct radeon_pcie *pcie,
					       unsigned long page,
					       uint32_t page_base)
{
	if (pcie == NULL || pcie->gart_table == NULL) {
		return;
	}
	pcie->gart_table[page] = cpu_to_le32((page_base >> 8) | 0xC);
}

static int pcie_ttm_bind(struct drm_ttm_backend *backend,
			 struct drm_bo_mem_reg *bo_mem)
{
	struct radeon_pcie_gart *pcie_gart;
	unsigned long page_first;
	unsigned long page_last;
	unsigned long page, i;
	uint32_t page_base;

	pcie_gart = container_of(backend, struct radeon_pcie_gart, backend);
	page = page_first = bo_mem->mm_node->start;
	page_last = page_first + pcie_gart->num_pages;
	if (page_first >= pcie_gart->pcie->page_last ||
	    page_last >= pcie_gart->pcie->page_last)
	    return -EINVAL;
        while (page < page_last) {
		if (pcie_gart_get_page_base(pcie_gart->pcie, page)) {
			return -EBUSY;
		}
                page++;
        }

        for (i = 0, page = page_first; i < pcie_gart->num_pages; i++, page++) {
		struct page *cur_page = pcie_gart->pages[i];

		if (!page) {
			cur_page = pcie_gart->dummy_read_page;
		}
                /* write value */
		page_base = page_to_phys(cur_page);
		pcie_gart_set_page_base(pcie_gart->pcie, page, page_base);
        }
	DRM_MEMORYBARRIER();
	pcie_gart_flush(pcie_gart->pcie);
	pcie_gart->bound = 1;
	pcie_gart->page_first = page_first;
	return 0;
}

static void pcie_ttm_clear(struct drm_ttm_backend *backend)
{
	struct radeon_pcie_gart *pcie_gart;

	pcie_gart = container_of(backend, struct radeon_pcie_gart, backend);
	if (pcie_gart->pages) {
		backend->func->unbind(backend);
		pcie_gart->pages = NULL;
	}
	pcie_gart->num_pages = 0;
}

static void pcie_ttm_destroy(struct drm_ttm_backend *backend)
{
	struct radeon_pcie_gart *pcie_gart;

	if (backend == NULL) {
		return;
	}
	pcie_gart = container_of(backend, struct radeon_pcie_gart, backend);
	if (pcie_gart->pages) {
		backend->func->clear(backend);
	}
	drm_ctl_free(pcie_gart, sizeof(*pcie_gart), DRM_MEM_TTM);
}

static int pcie_ttm_needs_ub_cache_adjust(struct drm_ttm_backend *backend)
{
	return ((backend->flags & DRM_BE_FLAG_BOUND_CACHED) ? 0 : 1);
}

static int pcie_ttm_populate(struct drm_ttm_backend *backend,
			     unsigned long num_pages, struct page **pages,
			     struct page *dummy_read_page)
{
	struct radeon_pcie_gart *pcie_gart;

	pcie_gart = container_of(backend, struct radeon_pcie_gart, backend);
	pcie_gart->pages = pages;
	pcie_gart->num_pages = num_pages;
	pcie_gart->populated = 1;
	return 0;
}

static int pcie_ttm_unbind(struct drm_ttm_backend *backend)
{
	struct radeon_pcie_gart *pcie_gart;
	unsigned long page, i;

	pcie_gart = container_of(backend, struct radeon_pcie_gart, backend);
	if (pcie_gart->bound != 1 || pcie_gart->pcie->gart_table == NULL) {
		return -EINVAL;
	}
	for (i = 0, page = pcie_gart->page_first; i < pcie_gart->num_pages;
	     i++, page++) {
	     	pcie_gart->pcie->gart_table[page] = 0;
	}
	pcie_gart_flush(pcie_gart->pcie);
	pcie_gart->bound = 0;
	pcie_gart->page_first = 0;
	return 0;
}

int radeon_ms_agp_finish(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (!dev_priv->bus_ready) {
		return 0;
	}
	dev_priv->bus_ready = 0;
	DRM_INFO("[radeon_ms] release agp\n");
	drm_agp_release(dev);
	return 0;
}

int radeon_ms_agp_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	struct drm_agp_mode mode;
	uint32_t agp_status;
	int ret;

	dev_priv->bus_ready = -1;
	if (dev->agp == NULL) {
		DRM_ERROR("[radeon_ms] can't initialize AGP\n");
		return -EINVAL;
	}
	ret = drm_agp_acquire(dev);
	if (ret) {
		DRM_ERROR("[radeon_ms] error failed to acquire agp %d\n", ret);
		return ret;
	}
	agp_status = MMIO_R(AGP_STATUS);
	if ((AGP_STATUS__MODE_AGP30 & agp_status)) {
		mode.mode = AGP_STATUS__RATE4X;
	} else {
		mode.mode = AGP_STATUS__RATE2X_8X;
	}
	ret = drm_agp_enable(dev, mode);
	if (ret) {
		DRM_ERROR("[radeon_ms] error failed to enable agp\n");
		return ret;
	}
	state->agp_command = MMIO_R(AGP_COMMAND) | AGP_COMMAND__AGP_EN;
	state->agp_command &= ~AGP_COMMAND__FW_EN;
	state->agp_command &= ~AGP_COMMAND__MODE_4G_EN;
	state->aic_ctrl = 0;
	state->agp_base = REG_S(AGP_BASE, AGP_BASE_ADDR, dev->agp->base);
	state->agp_base_2 = 0;
	state->bus_cntl = MMIO_R(BUS_CNTL);
	state->bus_cntl &= ~BUS_CNTL__BUS_MASTER_DIS;
	state->mc_agp_location =
		REG_S(MC_AGP_LOCATION, MC_AGP_START,
				dev_priv->gpu_gart_start >> 16) |
		REG_S(MC_AGP_LOCATION, MC_AGP_TOP,
				dev_priv->gpu_gart_end >> 16);
	DRM_INFO("[radeon_ms] gpu agp base 0x%08X\n", MMIO_R(AGP_BASE));
	DRM_INFO("[radeon_ms] gpu agp location 0x%08X\n",
		 MMIO_R(MC_AGP_LOCATION));
	DRM_INFO("[radeon_ms] gpu agp location 0x%08X\n",
		 state->mc_agp_location);
	DRM_INFO("[radeon_ms] bus ready\n");
	dev_priv->bus_ready = 1;
	return 0;
}

void radeon_ms_agp_restore(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	MMIO_W(MC_AGP_LOCATION, state->mc_agp_location);
	MMIO_W(AGP_BASE, state->agp_base);
	MMIO_W(AGP_BASE_2, state->agp_base_2);
	MMIO_W(AGP_COMMAND, state->agp_command);
}

void radeon_ms_agp_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	state->agp_command = MMIO_R(AGP_COMMAND);
	state->agp_base = MMIO_R(AGP_BASE);
	state->agp_base_2 = MMIO_R(AGP_BASE_2);
	state->mc_agp_location = MMIO_R(MC_AGP_LOCATION);
}

struct drm_ttm_backend *radeon_ms_pcie_create_ttm(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_pcie_gart *pcie_gart;

	pcie_gart = drm_ctl_calloc(1, sizeof (*pcie_gart), DRM_MEM_TTM);
	if (pcie_gart == NULL) {
		return NULL;
	}
	memset(pcie_gart, 0, sizeof(struct radeon_pcie_gart));
	pcie_gart->populated = 0;
	pcie_gart->pcie = dev_priv->bus;
	pcie_gart->backend.func = &radeon_pcie_gart_ttm_backend;

	return &pcie_gart->backend;
}

int radeon_ms_pcie_finish(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_pcie *pcie = dev_priv->bus;

	if (!dev_priv->bus_ready || pcie == NULL) {
		dev_priv->bus_ready = 0;
		return 0;
	}
	dev_priv->bus_ready = 0;
	if (pcie->gart_table) {
		drm_mem_reg_iounmap(dev, &pcie->gart_table_object->mem,
				    (void *)pcie->gart_table);
	}
	pcie->gart_table = NULL;
	if (pcie->gart_table_object) {
		mutex_lock(&dev->struct_mutex);
		drm_bo_usage_deref_locked(&pcie->gart_table_object);
		mutex_unlock(&dev->struct_mutex);
	}
	dev_priv->bus = NULL;
	drm_free(pcie, sizeof(*pcie), DRM_MEM_DRIVER);
	return 0;
}

int radeon_ms_pcie_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	struct radeon_pcie *pcie;
	int ret = 0;

	dev_priv->bus_ready = -1;
	/* allocate and clear device private structure */
	pcie = drm_alloc(sizeof(struct radeon_pcie), DRM_MEM_DRIVER);
	if (pcie == NULL) {
		return -ENOMEM;
	}
	memset(pcie, 0, sizeof(struct radeon_pcie));
	pcie->dev = dev;
	dev_priv->bus = (void *)pcie;
	pcie->gart_table_size = (dev_priv->gpu_gart_size / RADEON_PAGE_SIZE) *
				4;
	/* gart table start must be aligned on 16bytes, align it on one page */
	ret = drm_buffer_object_create(dev,
				       pcie->gart_table_size,
				       drm_bo_type_kernel,
				       DRM_BO_FLAG_READ |
				       DRM_BO_FLAG_WRITE |
				       DRM_BO_FLAG_MEM_VRAM |
				       DRM_BO_FLAG_NO_EVICT,
				       DRM_BO_HINT_DONT_FENCE,
				       1,
				       0,
				       &pcie->gart_table_object);
	if (ret) {
		return ret;
	}
	ret = drm_mem_reg_ioremap(dev, &pcie->gart_table_object->mem,
				  (void **) &pcie->gart_table);
	if (ret) {
		DRM_ERROR("[radeon_ms] error mapping gart table: %d\n", ret);
		return ret;
	}
	DRM_INFO("[radeon_ms] gart table in vram at 0x%08lX\n",
		 pcie->gart_table_object->offset);
	memset((void *)pcie->gart_table, 0, pcie->gart_table_size);
	pcie->page_last = pcie->gart_table_size >> 2;
	state->pcie_tx_gart_discard_rd_addr_lo =
		REG_S(PCIE_TX_GART_DISCARD_RD_ADDR_LO,
				GART_DISCARD_RD_ADDR_LO,
				dev_priv->gpu_gart_start);
	state->pcie_tx_gart_discard_rd_addr_hi =
		REG_S(PCIE_TX_GART_DISCARD_RD_ADDR_HI,
				GART_DISCARD_RD_ADDR_HI, 0);
	state->pcie_tx_gart_base =
		REG_S(PCIE_TX_GART_BASE, GART_BASE,
				pcie->gart_table_object->offset);
	state->pcie_tx_gart_start_lo =
		REG_S(PCIE_TX_GART_START_LO, GART_START_LO,
				dev_priv->gpu_gart_start);
	state->pcie_tx_gart_start_hi =
		REG_S(PCIE_TX_GART_START_HI, GART_START_HI, 0);
	state->pcie_tx_gart_end_lo =
		REG_S(PCIE_TX_GART_END_LO, GART_END_LO, dev_priv->gpu_gart_end);
	state->pcie_tx_gart_end_hi =
		REG_S(PCIE_TX_GART_END_HI, GART_END_HI, 0);
	/* FIXME: why this ? */
	state->aic_ctrl = 0;
	state->agp_base = 0; 
	state->agp_base_2 = 0; 
	state->bus_cntl = MMIO_R(BUS_CNTL);
	state->mc_agp_location = REG_S(MC_AGP_LOCATION, MC_AGP_START, 0xffc0) |
				 REG_S(MC_AGP_LOCATION, MC_AGP_TOP, 0xffff);
	state->pcie_tx_gart_cntl =
		PCIE_TX_GART_CNTL__GART_EN |
		REG_S(PCIE_TX_GART_CNTL, GART_UNMAPPED_ACCESS,
				GART_UNMAPPED_ACCESS__DISCARD) |
		REG_S(PCIE_TX_GART_CNTL, GART_MODE, GART_MODE__CACHE_32x128) |
		REG_S(PCIE_TX_GART_CNTL, GART_RDREQPATH_SEL,
				GART_RDREQPATH_SEL__HDP);
	DRM_INFO("[radeon_ms] gpu gart start 0x%08X\n",
		 PCIE_R(PCIE_TX_GART_START_LO));
	DRM_INFO("[radeon_ms] gpu gart end   0x%08X\n",
		 PCIE_R(PCIE_TX_GART_END_LO));
	DRM_INFO("[radeon_ms] bus ready\n");
	dev_priv->bus_ready = 1;
	return 0;
}

void radeon_ms_pcie_restore(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	/* disable gart before programing other registers */
	radeon_ms_agp_restore(dev, state);
	PCIE_W(PCIE_TX_GART_CNTL, 0);
	PCIE_W(PCIE_TX_GART_BASE, state->pcie_tx_gart_base);
	PCIE_W(PCIE_TX_GART_BASE, state->pcie_tx_gart_base);
	PCIE_W(PCIE_TX_GART_DISCARD_RD_ADDR_HI,
	       state->pcie_tx_gart_discard_rd_addr_hi);
	PCIE_W(PCIE_TX_GART_DISCARD_RD_ADDR_LO,
	       state->pcie_tx_gart_discard_rd_addr_lo);
	PCIE_W(PCIE_TX_GART_START_HI, state->pcie_tx_gart_start_hi);
	PCIE_W(PCIE_TX_GART_START_LO, state->pcie_tx_gart_start_lo);
	PCIE_W(PCIE_TX_GART_END_HI, state->pcie_tx_gart_end_hi);
	PCIE_W(PCIE_TX_GART_END_LO, state->pcie_tx_gart_end_lo);
	PCIE_W(PCIE_TX_GART_CNTL, state->pcie_tx_gart_cntl);
}

void radeon_ms_pcie_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	radeon_ms_agp_save(dev, state);
	state->pcie_tx_gart_base = PCIE_R(PCIE_TX_GART_BASE);
	state->pcie_tx_gart_base = PCIE_R(PCIE_TX_GART_BASE);
	state->pcie_tx_gart_discard_rd_addr_hi =
		PCIE_R(PCIE_TX_GART_DISCARD_RD_ADDR_HI);
	state->pcie_tx_gart_discard_rd_addr_lo =
		PCIE_R(PCIE_TX_GART_DISCARD_RD_ADDR_LO);
	state->pcie_tx_gart_start_hi = PCIE_R(PCIE_TX_GART_START_HI);
	state->pcie_tx_gart_start_lo = PCIE_R(PCIE_TX_GART_START_LO);
	state->pcie_tx_gart_end_hi = PCIE_R(PCIE_TX_GART_END_HI);
	state->pcie_tx_gart_end_lo = PCIE_R(PCIE_TX_GART_END_LO);
	state->pcie_tx_gart_cntl = PCIE_R(PCIE_TX_GART_CNTL);
}

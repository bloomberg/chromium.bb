/*
 * Copyright 2007 Jérôme Glisse
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
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#ifndef __RADEON_MS_PROPERTIES_H__
#define __RADEON_MS_PROPERTIES_H__

#define RADEON_PAGE_SIZE        4096
#define RADEON_MAX_CONNECTORS   8
#define RADEON_MAX_OUTPUTS      8

struct radeon_ms_properties {
	uint16_t                    subvendor;
	uint16_t                    subdevice;
	int16_t                     pll_reference_freq;
	int16_t                     pll_reference_div;
	int32_t                     pll_min_pll_freq;
	int32_t                     pll_max_pll_freq;
	char                        pll_use_bios;
	char                        pll_dummy_reads;
	char                        pll_delay;
	char                        pll_r300_errata;
	struct radeon_ms_output     *outputs[RADEON_MAX_OUTPUTS];
	struct radeon_ms_connector  *connectors[RADEON_MAX_CONNECTORS];
};

#endif

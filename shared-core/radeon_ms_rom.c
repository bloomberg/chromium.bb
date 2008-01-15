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
#include "radeon_ms.h"

int radeon_ms_rom_get_properties(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	switch (dev_priv->rom.type) {
	case ROM_COMBIOS:
		return radeon_ms_combios_get_properties(dev);
	}
	return 0;
}

int radeon_ms_rom_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_ms_rom *rom = &dev_priv->rom;
	void *rom_mapped;
	char atomstr[5] = {0, 0, 0, 0, 0};
	uint16_t *offset;

	dev_priv->rom.type = ROM_UNKNOWN;
	/* copy rom if any */
	rom_mapped = pci_map_rom_copy(dev->pdev, &rom->rom_size);
	if (rom->rom_size) {
		rom->rom_image = drm_alloc(rom->rom_size, DRM_MEM_DRIVER);
		if (rom->rom_image == NULL) {
			return -1;
		}
		memcpy(rom->rom_image, rom_mapped, rom->rom_size);
		DRM_INFO("[radeon_ms] ROM %d bytes copied\n", rom->rom_size);
	} else {
		DRM_INFO("[radeon_ms] no ROM\n");
		return 0;
	}
	pci_unmap_rom(dev->pdev, rom_mapped);

	if (rom->rom_image[0] != 0x55 || rom->rom_image[1] != 0xaa) {
		DRM_INFO("[radeon_ms] no ROM\n");
		DRM_INFO("[radeon_ms] ROM signature 0x55 0xaa missing\n");
		return 0;
	}
	offset = (uint16_t *)&rom->rom_image[ROM_HEADER];
	memcpy(atomstr, &rom->rom_image[*offset + 4], 4);
	if (!strcpy(atomstr, "ATOM") || !strcpy(atomstr, "MOTA")) {
		DRM_INFO("[radeon_ms] ATOMBIOS ROM detected\n");
		return 0;
	} else {
		struct combios_header **header;
		
		header = &rom->rom.combios_header;
		if ((*offset + sizeof(struct combios_header)) > rom->rom_size) {
			DRM_INFO("[radeon_ms] wrong COMBIOS header offset\n");
			return -1;
		}
		dev_priv->rom.type = ROM_COMBIOS;
		*header = (struct combios_header *)&rom->rom_image[*offset];
		DRM_INFO("[radeon_ms] COMBIOS type  : %d\n",
		         (*header)->ucTypeDefinition);
		DRM_INFO("[radeon_ms] COMBIOS  OEM ID: %02x %02x\n",
		         (*header)->ucOemID1, (*header)->ucOemID2);
	}
	return 0;
}

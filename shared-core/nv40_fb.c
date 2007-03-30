#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

int
nv40_fb_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fb_bar_size, tmp;
	int num_tiles;
	int i;

	switch (dev_priv->chipset) {
	case 0x40:
	case 0x45:
		tmp = NV_READ(NV10_PFB_CLOSE_PAGE2);
		NV_WRITE(NV10_PFB_CLOSE_PAGE2, tmp & ~(1<<15));
		num_tiles = NV10_PFB_TILE__SIZE;
		break;
	case 0x46: /* G72 */
	case 0x47: /* G70 */
	case 0x49: /* G71 */
	case 0x4b: /* G73 */
	case 0x4c: /* C51 (G7X version) */
		num_tiles = NV40_PFB_TILE__SIZE_1;
		break;
	default:
		num_tiles = NV40_PFB_TILE__SIZE_0;
		break;
	}

	fb_bar_size = drm_get_resource_len(dev, 0) - 1;
	switch (dev_priv->chipset) {
	case 0x40:
		for (i=0; i<num_tiles; i++) {
			NV_WRITE(NV10_PFB_TILE(i), 0);
			NV_WRITE(NV10_PFB_TLIMIT(i), fb_bar_size);
		}
		break;
	default:
		for (i=0; i<num_tiles; i++) {
			NV_WRITE(NV40_PFB_TILE(i), 0);
			NV_WRITE(NV40_PFB_TLIMIT(i), fb_bar_size);
		}
		break;
	}

	return 0;
}

void
nv40_fb_takedown(drm_device_t *dev)
{
}


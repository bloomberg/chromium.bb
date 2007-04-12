#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

int
nv04_mc_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/* Power up everything, resetting each individual unit will
	 * be done later if needed.
	 */
	NV_WRITE(NV03_PMC_ENABLE, 0xFFFFFFFF);

	NV_WRITE(NV03_PMC_INTR_EN_0, 0);

	return 0;
}

void
nv04_mc_takedown(drm_device_t *dev)
{
}


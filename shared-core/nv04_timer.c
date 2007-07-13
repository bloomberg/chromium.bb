#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

int
nv04_timer_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	NV_WRITE(NV04_PTIMER_INTR_EN_0, 0x00000000);
	NV_WRITE(NV04_PTIMER_INTR_0, 0xFFFFFFFF);

	NV_WRITE(NV04_PTIMER_NUMERATOR, 0x00000008);
	NV_WRITE(NV04_PTIMER_DENOMINATOR, 0x00000003);

	return 0;
}

void
nv04_timer_takedown(struct drm_device *dev)
{
}


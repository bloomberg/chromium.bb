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

uint64_t
nv04_timer_read(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t low;
	/* From kmmio dumps on nv28 this looks like how the blob does this.
	 * It reads the high dword twice, before and after.
	 * The only explanation seems to be that the 64-bit timer counter
	 * advances between high and low dword reads and may corrupt the
	 * result. Not confirmed.
	 */
	uint32_t high2 = NV_READ(NV04_PTIMER_TIME_1);
	uint32_t high1;
	do {
		high1 = high2;
		low = NV_READ(NV04_PTIMER_TIME_0);
		high2 = NV_READ(NV04_PTIMER_TIME_1);
	} while(high1 != high2);
	return (((uint64_t)high2) << 32) | (uint64_t)low;
}

void
nv04_timer_takedown(struct drm_device *dev)
{
}


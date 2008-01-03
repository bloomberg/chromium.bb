#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

int
nv04_mc_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t saved_pci_nv_1, saved_pci_nv_19;

	saved_pci_nv_1 = NV_READ(NV04_PBUS_PCI_NV_1);
	saved_pci_nv_19 = NV_READ(NV04_PBUS_PCI_NV_19);

	/* clear busmaster bit */
	NV_WRITE(NV04_PBUS_PCI_NV_1, saved_pci_nv_1 & ~(0x00000001 << 2));
	/* clear SBA, AGP and FW bits */
	NV_WRITE(NV04_PBUS_PCI_NV_19, saved_pci_nv_19 & 0xfffff00f);

	/* Power up everything, resetting each individual unit will
	 * be done later if needed.
	 */
	NV_WRITE(NV03_PMC_ENABLE, 0xFFFFFFFF);

	/* and restore (gives effect of resetting AGP) */
	NV_WRITE(NV04_PBUS_PCI_NV_19, saved_pci_nv_19);
	NV_WRITE(NV04_PBUS_PCI_NV_1, saved_pci_nv_1);

	return 0;
}

void
nv04_mc_takedown(struct drm_device *dev)
{
}

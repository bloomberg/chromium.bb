/* i915_pm.c -- Power management support for the i915 -*- linux-c -*-
 */
/**************************************************************************
 * 
 * Copyright 2004 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 **************************************************************************/

#define __NO_VERSION__
#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"


/** 
 * Set DPMS mode. 
 */
static int i915_set_dpms(drm_device_t *dev, int mode)
{
	drm_i915_private_t *dev_priv =
		(drm_i915_private_t *)dev->dev_private;
	unsigned sr01, adpa, ppcr, dvob, dvoc, lvds;
	
	DRM_DEBUG("%s mode=%d\n", __FUNCTION__, mode);

	I915_WRITE( SRX_INDEX, SR01 );
	sr01 = I915_READ( SRX_DATA );

	adpa = I915_READ( ADPA );
	ppcr = I915_READ( PPCR );
	dvoc = I915_READ( DVOC );
	dvob = I915_READ( DVOB );
	lvds = I915_READ( LVDS );

	switch(mode) {
		case 0:
			/* On */
			sr01 &= ~SR01_SCREEN_OFF;
			adpa = (adpa & ADPA_DPMS_MASK) | ADPA_DPMS_ON;
#if 0
			I915_WRITE( LVDS, lvds | LVDS_ON ); /* Power on LVDS */
#endif
			I915_WRITE( PPCR, dev_priv->ppcr ); /* Power up panel */
			I915_WRITE( DVOC, dev_priv->dvoc );
			I915_WRITE( DVOB, dev_priv->dvob );
			break;

		case 1:
			/* Standby */
			sr01 |= SR01_SCREEN_OFF; 
			adpa = (adpa & ADPA_DPMS_MASK) | ADPA_DPMS_STANDBY;
			I915_WRITE( PPCR, ppcr & ~PPCR_ON ); /* Power off panel*/
#if 0
			I915_WRITE( LVDS, lvds & ~LVDS_ON ); /* Power off LVDS */
#endif
			I915_WRITE( DVOC, dvoc & ~DVOC_ON );
			I915_WRITE( DVOB, dvob & ~DVOB_ON );
			break;

		case 2:
			/* Suspend */
			sr01 |= SR01_SCREEN_OFF; 
			adpa = (adpa & ADPA_DPMS_MASK) | ADPA_DPMS_SUSPEND;
			I915_WRITE( PPCR, ppcr & ~PPCR_ON ); /* Power off panel*/
#if 0
			I915_WRITE( LVDS, lvds & ~LVDS_ON ); /* Power off LVDS */
#endif
			I915_WRITE( DVOC, dvoc & ~DVOC_ON );
			I915_WRITE( DVOB, dvob & ~DVOB_ON );
			break;

		case 3:
			/* Off */
			sr01 |= SR01_SCREEN_OFF; 
			adpa = (adpa & ADPA_DPMS_MASK) | ADPA_DPMS_OFF;
			I915_WRITE( PPCR, ppcr & ~PPCR_ON ); /* Power off panel*/
#if 0
			I915_WRITE( LVDS, lvds & ~LVDS_ON ); /* Power off LVDS */
#endif
			I915_WRITE( DVOC, dvoc & ~DVOC_ON );
			I915_WRITE( DVOB, dvob & ~DVOB_ON );
			break;
	}
	
	I915_WRITE( SRX_DATA, sr01 );
 	
	I915_WRITE( ADPA, adpa );

	return 0;
}

int i915_suspend( struct pci_dev *pdev, unsigned state ) 
{
	drm_device_t *dev = (drm_device_t *)pci_get_drvdata(pdev);

	DRM_DEBUG("%s state=%d\n", __FUNCTION__, state);

	switch(state) {
		case 0:
			/* D0: set DPMS mode on */
			i915_set_dpms(dev, 0);
			break;
		case 1:
			/* D1: set DPMS mode standby */
			i915_set_dpms(dev, 1);
			break;
		case 2:
			/* D2: set DPMS mode suspend */
			i915_set_dpms(dev, 2);
			break;
		case 3:
			/* D3: set DPMS mode off */
			i915_set_dpms(dev, 3);
			break;
	}

	return 0;
}

int i915_resume( struct pci_dev *pdev ) 
{
	drm_device_t *dev = (drm_device_t *)pci_get_drvdata(pdev);

	/* D0: set DPMS mode on */
	i915_set_dpms(dev, 0);

	return 0;
}

int i915_power( drm_device_t *dev, unsigned int state )
{
	DRM_DEBUG("%s state=%d\n", __FUNCTION__, state);

	/* D0: set DPMS mode on */
	i915_set_dpms(dev, state);

	return 0;
}

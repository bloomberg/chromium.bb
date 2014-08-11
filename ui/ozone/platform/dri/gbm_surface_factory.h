// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRI_GBM_SURFACE_FACTORY_H_

#include "ui/ozone/platform/dri/dri_surface_factory.h"

struct gbm_device;

namespace ui {

class GbmSurfaceFactory : public DriSurfaceFactory {
 public:
  GbmSurfaceFactory(bool allow_surfaceless);
  virtual ~GbmSurfaceFactory();

  void InitializeGpu(DriWrapper* dri,
                     gbm_device* device,
                     ScreenManager* screen_manager);

  // DriSurfaceFactory:
  virtual intptr_t GetNativeDisplay() OVERRIDE;
  virtual const int32_t* GetEGLSurfaceProperties(
      const int32_t* desired_list) OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;
  virtual scoped_ptr<ui::SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget w) OVERRIDE;
  virtual scoped_refptr<ui::NativePixmap> CreateNativePixmap(
      gfx::Size size,
      BufferFormat format) OVERRIDE;
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    scoped_refptr<NativePixmap> buffer,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& crop_rect) OVERRIDE;
  virtual bool CanShowPrimaryPlaneAsOverlay() OVERRIDE;

 private:
  gbm_device* device_;  // Not owned.
  bool allow_surfaceless_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_SURFACE_FACTORY_H_

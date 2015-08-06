// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_FACTORY_H_

#include "ui/ozone/platform/drm/gpu/drm_surface_factory.h"

namespace ui {

class DrmDeviceManager;
class DrmWindow;
class GbmDevice;
class ScreenManager;

class GbmSurfaceFactory : public DrmSurfaceFactory {
 public:
  GbmSurfaceFactory(bool allow_surfaceless);
  ~GbmSurfaceFactory() override;

  void InitializeGpu(DrmDeviceManager* drm_device_manager,
                     ScreenManager* screen_manager);

  // DrmSurfaceFactory:
  intptr_t GetNativeDisplay() override;
  const int32_t* GetEGLSurfaceProperties(const int32_t* desired_list) override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;
  scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_ptr<ui::SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget w) override;
  scoped_ptr<SurfaceOzoneEGL> CreateSurfacelessEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<ui::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  bool CanShowPrimaryPlaneAsOverlay() override;

 private:
  scoped_refptr<GbmDevice> GetGbmDevice(gfx::AcceleratedWidget widget);

  bool allow_surfaceless_;

  DrmDeviceManager* drm_device_manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_FACTORY_H_

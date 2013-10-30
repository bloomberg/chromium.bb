// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_IMPL_SOFTWARE_SURFACE_FACTORY_OZONE_H_
#define UI_GFX_OZONE_IMPL_SOFTWARE_SURFACE_FACTORY_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"

namespace gfx {

class DrmWrapperOzone;
class HardwareDisplayControllerOzone;
class SoftwareSurfaceOzone;

// SurfaceFactoryOzone implementation on top of DRM/KMS using dumb buffers.
// This implementation is used in conjunction with the software rendering
// path.
class SoftwareSurfaceFactoryOzone : public SurfaceFactoryOzone {
 public:
  SoftwareSurfaceFactoryOzone();
  virtual ~SoftwareSurfaceFactoryOzone();

  virtual HardwareState InitializeHardware() OVERRIDE;
  virtual void ShutdownHardware() OVERRIDE;

  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual gfx::AcceleratedWidget RealizeAcceleratedWidget(
      gfx::AcceleratedWidget w) OVERRIDE;

  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

  virtual bool AttemptToResizeAcceleratedWidget(
      gfx::AcceleratedWidget w,
      const gfx::Rect& bounds) OVERRIDE;

  virtual bool SchedulePageFlip(gfx::AcceleratedWidget w) OVERRIDE;

  virtual SkCanvas* GetCanvasForWidget(gfx::AcceleratedWidget w) OVERRIDE;

  virtual gfx::VSyncProvider* GetVSyncProvider(
      gfx::AcceleratedWidget w) OVERRIDE;

 private:
  virtual SoftwareSurfaceOzone* CreateSurface(
    HardwareDisplayControllerOzone* controller);

  virtual DrmWrapperOzone* CreateWrapper();

  virtual bool InitializeControllerForPrimaryDisplay(
    DrmWrapperOzone* drm,
    HardwareDisplayControllerOzone* controller);

  // Blocks until a DRM event is read.
  // TODO(dnicoara) Remove once we can safely move DRM event processing in the
  // message loop while correctly signaling when we're done displaying the
  // pending frame.
  virtual void WaitForPageFlipEvent(int fd);

  scoped_ptr<DrmWrapperOzone> drm_;

  HardwareState state_;

  // Active output.
  scoped_ptr<HardwareDisplayControllerOzone> controller_;


  DISALLOW_COPY_AND_ASSIGN(SoftwareSurfaceFactoryOzone);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_IMPL_SOFTWARE_SURFACE_FACTORY_OZONE_H_

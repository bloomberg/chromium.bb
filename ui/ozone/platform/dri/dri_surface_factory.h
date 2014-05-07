// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/ozone/ozone_export.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class SurfaceOzoneCanvas;
}

namespace ui {

class DriSurface;
class DriWrapper;
class HardwareDisplayController;

// SurfaceFactoryOzone implementation on top of DRM/KMS using dumb buffers.
// This implementation is used in conjunction with the software rendering
// path.
class OZONE_EXPORT DriSurfaceFactory : public gfx::SurfaceFactoryOzone {
 public:
  static const gfx::AcceleratedWidget kDefaultWidgetHandle;

  DriSurfaceFactory();
  virtual ~DriSurfaceFactory();

  // SurfaceFactoryOzone overrides:
  virtual HardwareState InitializeHardware() OVERRIDE;
  virtual void ShutdownHardware() OVERRIDE;

  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;

  virtual scoped_ptr<gfx::SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget w) OVERRIDE;

  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

  virtual bool SchedulePageFlip(gfx::AcceleratedWidget w);

  virtual SkCanvas* GetCanvasForWidget(gfx::AcceleratedWidget w);

  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider(
      gfx::AcceleratedWidget w);

  gfx::Size GetWidgetSize(gfx::AcceleratedWidget w);

  void SetHardwareCursor(gfx::AcceleratedWidget window,
                         const SkBitmap& image,
                         const gfx::Point& location);

  void MoveHardwareCursor(gfx::AcceleratedWidget window,
                          const gfx::Point& location);

  void UnsetHardwareCursor(gfx::AcceleratedWidget window);

  // Called to initialize a new display. The display is then added to
  // |controllers_|. When GetAcceleratedWidget() is called it will get the next
  // available display from |controllers_|.
  bool CreateHardwareDisplayController(uint32_t connector,
                                       uint32_t crtc,
                                       const drmModeModeInfo& mode);

  void DestroyHardwareDisplayController(uint32_t connector, uint32_t crtc);

  bool DisableHardwareDisplayController(uint32_t crtc);

  bool IsWidgetValid(gfx::AcceleratedWidget w) const;

  // Returns the controller in |controllers_| associated with |w|.
  HardwareDisplayController* GetControllerForWidget(
      gfx::AcceleratedWidget w);

  DriWrapper* drm() const { return drm_.get(); }

 private:
  virtual DriSurface* CreateSurface(const gfx::Size& size);

  virtual DriWrapper* CreateWrapper();

  // On non CrOS builds there is no display configurator to look-up available
  // displays and initialize the HDCs. In such cases this is called internally
  // to initialize a display.
  virtual bool InitializePrimaryDisplay();

  // Blocks until a DRM event is read.
  // TODO(dnicoara) Remove once we can safely move DRM event processing in the
  // message loop while correctly signaling when we're done displaying the
  // pending frame.
  virtual void WaitForPageFlipEvent(int fd);

  // Draw the last set cursor & update the cursor plane.
  void ResetCursor(gfx::AcceleratedWidget w);

  scoped_ptr<DriWrapper> drm_;

  HardwareState state_;

  typedef std::map<gfx::AcceleratedWidget, HardwareDisplayController*>
      HardwareDisplayControllerMap;
  // Active outputs.
  HardwareDisplayControllerMap controllers_;
  int allocated_widgets_;
  int last_added_widget_;

  scoped_ptr<DriSurface> cursor_surface_;

  SkBitmap cursor_bitmap_;
  gfx::Point cursor_location_;

  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

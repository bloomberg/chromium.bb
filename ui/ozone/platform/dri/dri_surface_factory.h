// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class DriBuffer;
class DriWrapper;
class ScreenManager;
class SurfaceOzoneCanvas;

// SurfaceFactoryOzone implementation on top of DRM/KMS using dumb buffers.
// This implementation is used in conjunction with the software rendering
// path.
class DriSurfaceFactory : public ui::SurfaceFactoryOzone,
                          public HardwareCursorDelegate {
 public:
  static const gfx::AcceleratedWidget kDefaultWidgetHandle;

  DriSurfaceFactory(DriWrapper* drm, ScreenManager* screen_manager);
  virtual ~DriSurfaceFactory();

  // Describes the state of the hardware after initialization.
  enum HardwareState {
    UNINITIALIZED,
    INITIALIZED,
    FAILED,
  };

  // Open the display device.
  virtual HardwareState InitializeHardware();

  // Close the display device.
  virtual void ShutdownHardware();

  virtual scoped_ptr<ui::SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget w) OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

  // Create a new window/surface/widget identifier.
  gfx::AcceleratedWidget GetAcceleratedWidget();

  // Determine dimensions of a widget.
  gfx::Size GetWidgetSize(gfx::AcceleratedWidget w);

  // HardwareCursorDelegate:
  virtual void SetHardwareCursor(gfx::AcceleratedWidget window,
                                 const SkBitmap& image,
                                 const gfx::Point& location) OVERRIDE;
  virtual void MoveHardwareCursor(gfx::AcceleratedWidget window,
                                  const gfx::Point& location) OVERRIDE;

 protected:
  // Draw the last set cursor & update the cursor plane.
  void ResetCursor(gfx::AcceleratedWidget w);

  DriWrapper* drm_;  // Not owned.
  ScreenManager* screen_manager_;  // Not owned.
  HardwareState state_;

  // Active outputs.
  int allocated_widgets_;

  scoped_refptr<DriBuffer> cursor_buffers_[2];
  int cursor_frontbuffer_;

  SkBitmap cursor_bitmap_;
  gfx::Point cursor_location_;

  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

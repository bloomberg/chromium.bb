// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class DriBuffer;
class DriWindowDelegateManager;
class DriWrapper;
class ScreenManager;
class SurfaceOzoneCanvas;

// SurfaceFactoryOzone implementation on top of DRM/KMS using dumb buffers.
// This implementation is used in conjunction with the software rendering
// path.
class DriSurfaceFactory : public SurfaceFactoryOzone,
                          public HardwareCursorDelegate {
 public:
  static const gfx::AcceleratedWidget kDefaultWidgetHandle;

  DriSurfaceFactory(DriWrapper* drm,
                    ScreenManager* screen_manager,
                    DriWindowDelegateManager* window_manager);
  virtual ~DriSurfaceFactory();

  // Describes the state of the hardware after initialization.
  enum HardwareState {
    UNINITIALIZED,
    INITIALIZED,
    FAILED,
  };

  // Open the display device.
  HardwareState InitializeHardware();

  // Close the display device.
  void ShutdownHardware();

  // SurfaceFactoryOzone:
  virtual scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

  // HardwareCursorDelegate:
  virtual void SetHardwareCursor(gfx::AcceleratedWidget widget,
                                 const std::vector<SkBitmap>& bitmaps,
                                 const gfx::Point& location,
                                 int frame_delay_ms) OVERRIDE;
  virtual void MoveHardwareCursor(gfx::AcceleratedWidget window,
                                  const gfx::Point& location) OVERRIDE;

 protected:
  // Draw the last set cursor & update the cursor plane.
  void ResetCursor();

  // Draw next frame in an animated cursor.
  void OnCursorAnimationTimeout();

  DriWrapper* drm_;  // Not owned.
  ScreenManager* screen_manager_;  // Not owned.
  DriWindowDelegateManager* window_manager_;  // Not owned.
  HardwareState state_;

  scoped_refptr<DriBuffer> cursor_buffers_[2];
  int cursor_frontbuffer_;

  gfx::AcceleratedWidget cursor_widget_;
  std::vector<SkBitmap> cursor_bitmaps_;
  gfx::Point cursor_location_;
  int cursor_frame_;
  int cursor_frame_delay_ms_;
  base::RepeatingTimer<DriSurfaceFactory> cursor_timer_;

  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

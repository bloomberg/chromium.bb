// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_H_

#include <vector>

#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/drm/gpu/display_change_observer.h"

class SkBitmap;

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {

class DrmBuffer;
class DrmDeviceManager;
class HardwareDisplayController;
class ScreenManager;

// A delegate of the platform window (DrmWindow) on the GPU process. This is
// used to keep track of window state changes such that each platform window is
// correctly associated with a display.
// A window is associated with the display whose bounds contains the window
// bounds. If there's no suitable display, the window is disconnected and its
// contents will not be visible.
class OZONE_EXPORT DrmWindow : public DisplayChangeObserver {
 public:
  DrmWindow(gfx::AcceleratedWidget widget,
            DrmDeviceManager* device_manager,
            ScreenManager* screen_manager);

  ~DrmWindow() override;

  void Initialize();

  void Shutdown();

  // Returns the accelerated widget associated with the delegate.
  gfx::AcceleratedWidget GetAcceleratedWidget();

  // Returns the current controller the window is displaying on. Callers should
  // not cache the result as the controller may change as the window is moved.
  HardwareDisplayController* GetController();

  // Called when the window is resized/moved.
  void OnBoundsChanged(const gfx::Rect& bounds);

  // Update the HW cursor bitmap & move to specified location. If
  // the bitmap is empty, the cursor is hidden.
  void SetCursor(const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& location,
                 int frame_delay_ms);

  // Update the HW cursor bitmap & move to specified location. If
  // the bitmap is empty, the cursor is hidden.
  void SetCursorWithoutAnimations(const std::vector<SkBitmap>& bitmaps,
                                  const gfx::Point& location);

  // Move the HW cursor to the specified location.
  void MoveCursor(const gfx::Point& location);

  // DisplayChangeObserver:
  void OnDisplayChanged(HardwareDisplayController* controller) override;
  void OnDisplayRemoved(HardwareDisplayController* controller) override;

 private:
  // Draw the last set cursor & update the cursor plane.
  void ResetCursor(bool bitmap_only);

  // Draw next frame in an animated cursor.
  void OnCursorAnimationTimeout();

  void UpdateWidgetToDrmDeviceMapping();

  // When |controller_| changes this is called to reallocate the cursor buffers
  // since the allocation DRM device may have changed.
  void UpdateCursorBuffers();

  gfx::AcceleratedWidget widget_;

  DrmDeviceManager* device_manager_;  // Not owned.
  ScreenManager* screen_manager_;     // Not owned.

  // The current bounds of the window.
  gfx::Rect bounds_;

  // The controller associated with the current window. This may be nullptr if
  // the window isn't over an active display.
  HardwareDisplayController* controller_;

  base::RepeatingTimer<DrmWindow> cursor_timer_;

  scoped_refptr<DrmBuffer> cursor_buffers_[2];
  int cursor_frontbuffer_;

  std::vector<SkBitmap> cursor_bitmaps_;
  gfx::Point cursor_location_;
  int cursor_frame_;
  int cursor_frame_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(DrmWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_

#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/dri/display_change_observer.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"

namespace ui {

class DriBuffer;
class DrmDeviceManager;
class HardwareDisplayController;
class ScreenManager;

class OZONE_EXPORT DriWindowDelegateImpl : public DriWindowDelegate,
                                           public DisplayChangeObserver {
 public:
  DriWindowDelegateImpl(gfx::AcceleratedWidget widget,
                        DrmDeviceManager* device_manager,
                        ScreenManager* screen_manager);
  ~DriWindowDelegateImpl() override;

  // DriWindowDelegate:
  void Initialize() override;
  void Shutdown() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  HardwareDisplayController* GetController() override;
  void OnBoundsChanged(const gfx::Rect& bounds) override;
  void SetCursor(const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& location,
                 int frame_delay_ms) override;
  void SetCursorWithoutAnimations(const std::vector<SkBitmap>& bitmaps,
                                  const gfx::Point& location) override;
  void MoveCursor(const gfx::Point& location) override;

  // DisplayChangeObserver:
  void OnDisplayChanged(HardwareDisplayController* controller) override;
  void OnDisplayRemoved(HardwareDisplayController* controller) override;

 private:
  // Draw the last set cursor & update the cursor plane.
  void ResetCursor(bool bitmap_only);

  // Draw next frame in an animated cursor.
  void OnCursorAnimationTimeout();

  void UpdateWidgetToDrmDeviceMapping();

  gfx::AcceleratedWidget widget_;

  DrmDeviceManager* device_manager_;          // Not owned.
  ScreenManager* screen_manager_;             // Not owned.

  // The current bounds of the window.
  gfx::Rect bounds_;

  // The controller associated with the current window. This may be nullptr if
  // the window isn't over an active display.
  HardwareDisplayController* controller_;

  base::RepeatingTimer<DriWindowDelegateImpl> cursor_timer_;

  scoped_refptr<DriBuffer> cursor_buffers_[2];
  int cursor_frontbuffer_;

  std::vector<SkBitmap> cursor_bitmaps_;
  gfx::Point cursor_location_;
  int cursor_frame_;
  int cursor_frame_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(DriWindowDelegateImpl);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_

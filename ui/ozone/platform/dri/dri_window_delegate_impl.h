// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class DriBuffer;
class DriWindowDelegateManager;
class DriWrapper;
class HardwareDisplayController;
class ScreenManager;

class DriWindowDelegateImpl : public DriWindowDelegate {
 public:
  DriWindowDelegateImpl(gfx::AcceleratedWidget widget,
                        DriWrapper* drm,
                        DriWindowDelegateManager* window_manager,
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

 private:
  // Draw the last set cursor & update the cursor plane.
  void ResetCursor(bool bitmap_only);

  // Draw next frame in an animated cursor.
  void OnCursorAnimationTimeout();

  gfx::AcceleratedWidget widget_;

  DriWrapper* drm_;                           // Not owned.
  DriWindowDelegateManager* window_manager_;  // Not owned.
  ScreenManager* screen_manager_;             // Not owned.

  base::WeakPtr<HardwareDisplayController> controller_;

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

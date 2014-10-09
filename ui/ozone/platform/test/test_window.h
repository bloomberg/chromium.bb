// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_TEST_WINDOW_H_
#define UI_OZONE_PLATFORM_TEST_TEST_WINDOW_H_

#include "base/files/file_path.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class PlatformWindowDelegate;
class TestWindowManager;

class TestWindow : public PlatformWindow {
 public:
  TestWindow(PlatformWindowDelegate* delegate,
             TestWindowManager* manager,
             const gfx::Rect& bounds);
  virtual ~TestWindow();

  // Path for image file for this window.
  base::FilePath path();

  // PlatformWindow:
  virtual gfx::Rect GetBounds() override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void Close() override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual void ToggleFullscreen() override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual void Restore() override;
  virtual void SetCursor(PlatformCursor cursor) override;
  virtual void MoveCursorTo(const gfx::Point& location) override;

 private:
  PlatformWindowDelegate* delegate_;
  TestWindowManager* manager_;
  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_TEST_WINDOW_H_

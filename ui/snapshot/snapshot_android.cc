// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "base/callback.h"
#include "ui/base/android/view_android.h"
#include "ui/base/android/window_android.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  return GrabWindowSnapshot(
      view->GetWindowAndroid(), png_representation, snapshot_bounds);
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  gfx::Display display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  gfx::Rect scaled_bounds =
      gfx::ScaleToEnclosingRect(snapshot_bounds,
                                display.device_scale_factor());
  return window->GrabSnapshot(
      scaled_bounds.x(), scaled_bounds.y(), scaled_bounds.width(),
      scaled_bounds.height(), png_representation);
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    GrabWindowSnapshotAsyncCallback callback) {
  callback.Run(gfx::Image());
}

void GrabViewSnapshotAsync(
    gfx::NativeView view,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  callback.Run(scoped_refptr<base::RefCountedBytes>());
}

void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  callback.Run(scoped_refptr<base::RefCountedBytes>());
}

}  // namespace ui

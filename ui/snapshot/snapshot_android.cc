// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task_runner.h"
#include "cc/output/copy_output_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/snapshot/snapshot_async.h"

namespace ui {

// Sync versions are not supported in Android.  Callers should fall back
// to the async version.
bool GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& snapshot_bounds,
                      gfx::Image* image) {
  return GrabWindowSnapshot(view->GetWindowAndroid(), snapshot_bounds, image);
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        const gfx::Rect& snapshot_bounds,
                        gfx::Image* image) {
  return false;
}

static void MakeAsyncCopyRequest(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    cc::CopyOutputRequest::CopyOutputRequestCallback callback) {
  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(std::move(callback));

  float scale = ui::GetScaleFactorForNativeView(window);
  request->set_area(gfx::ScaleToEnclosingRect(source_rect, scale));
  window->GetCompositor()->RequestCopyOfOutputOnRootLayer(std::move(request));
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncCallback& callback) {
  MakeAsyncCopyRequest(
      window, source_rect,
      base::BindOnce(&SnapshotAsync::ScaleCopyOutputResult, callback,
                     target_size, background_task_runner));
}

void GrabWindowSnapshotAsync(gfx::NativeWindow window,
                             const gfx::Rect& source_rect,
                             const GrabWindowSnapshotAsyncCallback& callback) {
  MakeAsyncCopyRequest(
      window, source_rect,
      base::BindOnce(&SnapshotAsync::RunCallbackWithCopyOutputResult,
                     callback));
}

void GrabViewSnapshotAsync(gfx::NativeView view,
                           const gfx::Rect& source_rect,
                           const GrabWindowSnapshotAsyncCallback& callback) {
  MakeAsyncCopyRequest(
      view->GetWindowAndroid(), source_rect,
      base::BindOnce(&SnapshotAsync::RunCallbackWithCopyOutputResult,
                     callback));
}

}  // namespace ui

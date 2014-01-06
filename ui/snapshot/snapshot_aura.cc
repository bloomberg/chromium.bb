// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/safe_numerics.h"
#include "base/task_runner_util.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

void OnFrameScalingFinished(
    const GrabWindowSnapshotAsyncCallback& callback,
    const SkBitmap& scaled_bitmap) {
  callback.Run(gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(scaled_bitmap)));
}

void ScaleCopyOutputResult(
    const GrabWindowSnapshotAsyncCallback& callback,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback.Run(gfx::Image());
    return;
  }

  // There are two overrides for skia::ImageOperations::Resize(), so we need get
  // pointer to the right override explicitly (otherwise the base::Bind() call
  // below won't compile).
  SkBitmap (*resize_function)(const SkBitmap&,
                              skia::ImageOperations::ResizeMethod, int, int,
                              SkBitmap::Allocator* allocator) =
      &skia::ImageOperations::Resize;

  // TODO(sergeyu): Potentially images can be scaled on GPU before reading it
  // from GPU. Image scaling is implemented in content::GlHelper, but it's can't
  // be used here because it's not in content/public. Move the scaling code
  // somewhere so that it can be reused here.
  base::PostTaskAndReplyWithResult(
      background_task_runner, FROM_HERE,
      base::Bind(resize_function, *result->TakeBitmap(),
                 skia::ImageOperations::RESIZE_GOOD,
                 target_size.width(), target_size.height(),
                 static_cast<SkBitmap::Allocator*>(NULL)),
      base::Bind(&OnFrameScalingFinished, callback));
}

}  // namespace

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  return GrabWindowSnapshot(view, png_representation, snapshot_bounds);
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  ui::Compositor* compositor = window->layer()->GetCompositor();

  gfx::RectF read_pixels_bounds = snapshot_bounds;

  // We must take into account the window's position on the desktop.
  read_pixels_bounds.Offset(
      window->GetBoundsInRootWindow().origin().OffsetFromOrigin());
  aura::WindowEventDispatcher* dispatcher = window->GetDispatcher();
  if (dispatcher)
    dispatcher->host()->GetRootTransform().TransformRect(&read_pixels_bounds);

  gfx::Rect read_pixels_bounds_in_pixel =
      gfx::ToEnclosingRect(read_pixels_bounds);

  // Sometimes (i.e. when using Aero on Windows) the compositor's size is
  // smaller than the window bounds. So trim appropriately.
  read_pixels_bounds_in_pixel.Intersect(gfx::Rect(compositor->size()));

  DCHECK_LE(0, read_pixels_bounds.x());
  DCHECK_LE(0, read_pixels_bounds.y());

  SkBitmap bitmap;
  if (!compositor->ReadPixels(&bitmap, read_pixels_bounds_in_pixel))
    return false;

  gfx::Display display =
      gfx::Screen::GetScreenFor(window)->GetDisplayNearestWindow(window);
  switch (display.rotation()) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      bitmap = SkBitmapOperations::Rotate(
          bitmap, SkBitmapOperations::ROTATION_270_CW);
      break;
    case gfx::Display::ROTATE_180:
      bitmap = SkBitmapOperations::Rotate(
          bitmap, SkBitmapOperations::ROTATION_180_CW);
      break;
    case gfx::Display::ROTATE_270:
      bitmap = SkBitmapOperations::Rotate(
          bitmap, SkBitmapOperations::ROTATION_90_CW);
      break;
  }

  unsigned char* pixels = reinterpret_cast<unsigned char*>(
      bitmap.pixelRef()->pixels());
  return gfx::PNGCodec::Encode(
      pixels, gfx::PNGCodec::FORMAT_BGRA,
      gfx::Size(bitmap.width(), bitmap.height()),
      base::checked_numeric_cast<int>(bitmap.rowBytes()),
      true, std::vector<gfx::PNGCodec::Comment>(),
      png_representation);
}

SNAPSHOT_EXPORT void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncCallback& callback) {
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&ScaleCopyOutputResult, callback, target_size,
                     background_task_runner));
  request->set_area(ui::ConvertRectToPixel(window->layer(), source_rect));
  window->layer()->RequestCopyOfOutput(request.Pass());
}

}  // namespace ui

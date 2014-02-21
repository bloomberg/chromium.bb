// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task_runner_util.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
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

SkBitmap ScaleBitmap(const SkBitmap& input_bitmap,
                     const gfx::Size& target_size) {
  return skia::ImageOperations::Resize(
      input_bitmap,
      skia::ImageOperations::RESIZE_GOOD,
      target_size.width(),
      target_size.height(),
      static_cast<SkBitmap::Allocator*>(NULL));
}

scoped_refptr<base::RefCountedBytes> EncodeBitmap(const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);
  unsigned char* pixels =
      reinterpret_cast<unsigned char*>(bitmap.pixelRef()->pixels());
  if (!gfx::PNGCodec::Encode(pixels,
                             gfx::PNGCodec::FORMAT_BGRA,
                             gfx::Size(bitmap.width(), bitmap.height()),
                             base::checked_cast<int>(bitmap.rowBytes()),
                             true,
                             std::vector<gfx::PNGCodec::Comment>(),
                             &png_data->data())) {
    return scoped_refptr<base::RefCountedBytes>();
  }
  return png_data;
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

  // TODO(sergeyu): Potentially images can be scaled on GPU before reading it
  // from GPU. Image scaling is implemented in content::GlHelper, but it's can't
  // be used here because it's not in content/public. Move the scaling code
  // somewhere so that it can be reused here.
  base::PostTaskAndReplyWithResult(
      background_task_runner,
      FROM_HERE,
      base::Bind(ScaleBitmap, *result->TakeBitmap(), target_size),
      base::Bind(&OnFrameScalingFinished, callback));
}

void EncodeCopyOutputResult(
    const GrabWindowSnapshotAsyncPNGCallback& callback,
    scoped_refptr<base::TaskRunner> background_task_runner,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback.Run(scoped_refptr<base::RefCountedBytes>());
    return;
  }

  // TODO(sergeyu): Potentially images can be scaled on GPU before reading it
  // from GPU. Image scaling is implemented in content::GlHelper, but it's can't
  // be used here because it's not in content/public. Move the scaling code
  // somewhere so that it can be reused here.
  base::PostTaskAndReplyWithResult(background_task_runner,
                                   FROM_HERE,
                                   base::Bind(EncodeBitmap,
                                              *result->TakeBitmap()),
                                   callback);
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
  // Not supported in Aura.  Callers should fall back to the async version.
  return false;
}

void MakeAsyncCopyRequest(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const cc::CopyOutputRequest::CopyOutputRequestCallback& callback) {
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(callback);
  request->set_area(ui::ConvertRectToPixel(window->layer(), source_rect));
  window->layer()->RequestCopyOfOutput(request.Pass());
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncCallback& callback) {
  MakeAsyncCopyRequest(window,
                       source_rect,
                       base::Bind(&ScaleCopyOutputResult,
                                  callback,
                                  target_size,
                                  background_task_runner));
}

void GrabWindowSnapshotAsync(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  MakeAsyncCopyRequest(window,
                       source_rect,
                       base::Bind(&EncodeCopyOutputResult,
                                  callback,
                                  background_task_runner));
}

void GrabViewSnapshotAsync(
    gfx::NativeView view,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  GrabWindowSnapshotAsync(view, source_rect, background_task_runner, callback);
}


}  // namespace ui

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task_runner_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace ui {

namespace {

scoped_refptr<base::RefCountedMemory> EncodeImageAsPNG(
    const gfx::Image& image) {
  std::vector<uint8_t> result;
  DCHECK(!image.AsImageSkia().GetRepresentation(1.0f).is_null());
  gfx::PNGCodec::FastEncodeBGRASkBitmap(image.AsBitmap(), true, &result);
  return new base::RefCountedBytes(result);
}

scoped_refptr<base::RefCountedMemory> EncodeImageAsJPEG(
    const gfx::Image& image) {
  std::vector<uint8_t> result;
  DCHECK(!image.AsImageSkia().GetRepresentation(1.0f).is_null());
  gfx::JPEG1xEncodedDataFromImage(image, 100, &result);
  return new base::RefCountedBytes(result);
}

void EncodeImageAndScheduleCallback(
    scoped_refptr<base::RefCountedMemory> (*encode_func)(const gfx::Image&),
    scoped_refptr<base::TaskRunner> background_task_runner,
    const base::Callback<void(scoped_refptr<base::RefCountedMemory> data)>&
        callback,
    const gfx::Image& image) {
  base::PostTaskAndReplyWithResult(background_task_runner.get(), FROM_HERE,
                                   base::Bind(encode_func, image), callback);
}

}  // namespace

void GrabWindowSnapshotAsyncPNG(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  GrabWindowSnapshotAsync(
      window, source_rect,
      base::Bind(EncodeImageAndScheduleCallback, &EncodeImageAsPNG,
                 std::move(background_task_runner), callback));
}

void GrabWindowSnapshotAsyncJPEG(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncJPEGCallback& callback) {
  GrabWindowSnapshotAsync(
      window, source_rect,
      base::Bind(EncodeImageAndScheduleCallback, &EncodeImageAsJPEG,
                 std::move(background_task_runner), callback));
}

}  // namespace ui

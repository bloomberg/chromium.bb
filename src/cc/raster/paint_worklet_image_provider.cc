// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/paint_worklet_image_provider.h"

#include <utility>
#include "cc/tiles/paint_worklet_image_cache.h"

namespace cc {

PaintWorkletImageProvider::PaintWorkletImageProvider(
    PaintWorkletImageCache* cache)
    : cache_(cache) {
  DCHECK(cache_);
}

PaintWorkletImageProvider::~PaintWorkletImageProvider() = default;

PaintWorkletImageProvider::PaintWorkletImageProvider(
    PaintWorkletImageProvider&& other) = default;

PaintWorkletImageProvider& PaintWorkletImageProvider::operator=(
    PaintWorkletImageProvider&& other) = default;

ImageProvider::ScopedResult PaintWorkletImageProvider::GetPaintRecordResult(
    PaintWorkletInput* input) {
  std::pair<sk_sp<PaintRecord>, base::OnceCallback<void()>>
      record_and_callback = cache_->GetPaintRecordAndRef(input);
  return ImageProvider::ScopedResult(std::move(record_and_callback.first),
                                     std::move(record_and_callback.second));
}

}  // namespace cc

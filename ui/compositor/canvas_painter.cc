// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/canvas_painter.h"

#include "cc/playback/display_item_list.h"

namespace ui {

CanvasPainter::CanvasPainter(SkBitmap* output,
                             const gfx::Size& paint_size,
                             float raster_scale,
                             SkColor clear_color)
    : output_(output),
      paint_size_(paint_size),
      raster_scale_(raster_scale),
      clear_color_(clear_color),
      list_(new cc::DisplayItemList),
      context_(list_.get(), raster_scale, gfx::Rect(paint_size_)) {}

CanvasPainter::~CanvasPainter() {
  gfx::Size pixel_size = gfx::ScaleToCeiledSize(paint_size_, raster_scale_);
  SkImageInfo info = SkImageInfo::MakeN32(
      pixel_size.width(), pixel_size.height(), kPremul_SkAlphaType);
  if (!output_->tryAllocPixels(info))
    return;

  SkCanvas canvas(*output_);
  canvas.clear(clear_color_);

  canvas.scale(raster_scale_, raster_scale_);

  list_->Finalize();
  list_->Raster(&canvas, nullptr);
}

}  // namespace ui

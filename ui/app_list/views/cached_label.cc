// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/cached_label.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace app_list {

CachedLabel::CachedLabel()
    : needs_repaint_(true) {
}

void CachedLabel::PaintToBackingImage() {
  if (image_.size() == size() && !needs_repaint_)
    return;

  bool is_opaque = SkColorGetA(background_color()) == 0xFF;
  float scale_factor =
      ui::GetScaleFactorForNativeView(GetWidget()->GetNativeView());
  gfx::Canvas canvas(size(), scale_factor, is_opaque);
  // If a background is provided, it will initialize the canvas in
  // View::OnPaintBackground(). Otherwise, the background must be set here.
  if (!background()) {
    canvas.FillRect(
        GetLocalBounds(), background_color(), SkXfermode::kSrc_Mode);
  }
  Label::OnPaint(&canvas);
  image_ = gfx::ImageSkia(canvas.ExtractImageRep());
  needs_repaint_ = false;
}

#if defined(OS_WIN)
void CachedLabel::OnPaint(gfx::Canvas* canvas) {
  PaintToBackingImage();
  canvas->DrawImageInt(image_, 0, 0);
}
#endif

void CachedLabel::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  Invalidate();
  View::OnDeviceScaleFactorChanged(device_scale_factor);
}

}  // namespace app_list

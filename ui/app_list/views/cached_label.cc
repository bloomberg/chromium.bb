// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/cached_label.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"

namespace app_list {

CachedLabel::CachedLabel()
    : needs_repaint_(true) {
}

void CachedLabel::PaintToBackingImage() {
  if (image_.size() == size() && !needs_repaint_)
    return;

  bool is_opaque = SkColorGetA(background_color()) == 0xFF;
  gfx::Canvas canvas(size(), 1.0f, is_opaque);
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

}  // namespace app_list

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test/test_texture.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace ui {

// static
int TestTexture::live_count_ = 0;

TestTexture::TestTexture() {
  live_count_++;
}

TestTexture::~TestTexture() {
  live_count_--;
}

void TestTexture::SetCanvas(const SkCanvas& canvas,
                            const gfx::Point& origin,
                            const gfx::Size& overall_size) {
  const SkBitmap& bitmap = canvas.getDevice()->accessBitmap(false);
  bounds_of_last_paint_.SetRect(
      origin.x(), origin.y(), bitmap.width(), bitmap.height());
}

void TestTexture::Draw(const ui::TextureDrawParams& params,
                       const gfx::Rect& clip_bounds) {
}

}  // namespace ui

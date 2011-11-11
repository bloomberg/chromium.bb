// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas_skia.h"

#include "base/logging.h"
#include "ui/gfx/font.h"

namespace gfx {

// static
void CanvasSkia::SizeStringInt(const string16& text,
                               const gfx::Font& font,
                               int* width, int* height, int flags) {
  NOTIMPLEMENTED();
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               int x, int y, int w, int h,
                               int flags) {
  NOTIMPLEMENTED();
}

ui::TextureID CanvasSkia::GetTextureID() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace gfx

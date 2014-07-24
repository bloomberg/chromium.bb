// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

namespace gfx {

FontRenderParams::FontRenderParams()
    : antialiasing(true),
      subpixel_positioning(true),
      autohinter(false),
      use_bitmaps(false),
      hinting(HINTING_MEDIUM),
      subpixel_rendering(SUBPIXEL_RENDERING_NONE) {
}

FontRenderParams::~FontRenderParams() {}

}  // namespace gfx

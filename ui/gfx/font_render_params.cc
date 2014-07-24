// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

namespace gfx {

FontRenderParams::FontRenderParams()
    : antialiasing(true),
      subpixel_positioning(true),
      autohinter(true),
      use_bitmaps(true),
      hinting(HINTING_SLIGHT),
      subpixel_rendering(SUBPIXEL_RENDERING_RGB) {
}

FontRenderParams::~FontRenderParams() {}

}  // namespace gfx

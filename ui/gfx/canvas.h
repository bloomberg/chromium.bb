// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_H_
#define UI_GFX_CANVAS_H_
#pragma once

#include "ui/base/ui_export.h"
#include "ui/gfx/canvas_skia.h"

namespace gfx {

// TODO(tfarina): This is a temporary work around until we rename all the
// entries from CanvasSkia to Canvas.
class UI_EXPORT Canvas : public CanvasSkia {
 public:
  Canvas();

  explicit Canvas(SkCanvas* canvas);

  Canvas(const gfx::Size& size, bool is_opaque);

  Canvas(const SkBitmap& bitmap, bool is_opaque);
};

}  // namespace gfx

#endif  // UI_GFX_CANVAS_H_

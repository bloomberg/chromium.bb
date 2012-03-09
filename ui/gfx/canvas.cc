// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

namespace gfx {

Canvas::Canvas()
    : CanvasSkia() {
}

Canvas::Canvas(SkCanvas* canvas)
    : CanvasSkia(canvas) {
}

Canvas::Canvas(const gfx::Size& size, bool is_opaque)
    : CanvasSkia(size, is_opaque) {
}

Canvas::Canvas(const SkBitmap& bitmap, bool is_opaque)
    : CanvasSkia(bitmap, is_opaque) {
}

}  // namespace gfx

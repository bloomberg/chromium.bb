// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

namespace gfx {

CanvasSkia* Canvas::AsCanvasSkia() {
  return NULL;
}

const CanvasSkia* Canvas::AsCanvasSkia() const {
  return NULL;
}

SkCanvas* Canvas::GetSkCanvas() {
  return NULL;
}

const SkCanvas* Canvas::GetSkCanvas() const {
  return NULL;
}

}  // namespace gfx;

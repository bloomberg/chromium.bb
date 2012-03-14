// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/transform_util.h"

#include "ui/gfx/point.h"

namespace ui {

Transform GetScaleTransform(const gfx::Point& anchor, float scale) {
  ui::Transform transform;
  transform.ConcatScale(scale, scale);
  transform.ConcatTranslate(anchor.x() * (1 - scale),
                            anchor.y() * (1 - scale));
  return transform;
}

}  // namespace ui

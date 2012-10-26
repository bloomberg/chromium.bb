// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rect_conversions.h"

#include "ui/gfx/safe_integer_conversions.h"

namespace gfx {

Rect ToEnclosingRect(const RectF& rect) {
  int min_x = ToFlooredInt(rect.origin().x());
  int min_y = ToFlooredInt(rect.origin().y());
  float max_x = rect.origin().x() + rect.size().width();
  float max_y = rect.origin().y() + rect.size().height();
  int width = std::max(ToCeiledInt(max_x) - min_x, 0);
  int height = std::max(ToCeiledInt(max_y) - min_y, 0);
  return Rect(min_x, min_y, width, height);
}

Rect ToEnclosedRect(const RectF& rect) {
  int min_x = ToCeiledInt(rect.origin().x());
  int min_y = ToCeiledInt(rect.origin().y());
  float max_x = rect.origin().x() + rect.size().width();
  float max_y = rect.origin().y() + rect.size().height();
  int width = std::max(ToFlooredInt(max_x) - min_x, 0);
  int height = std::max(ToFlooredInt(max_y) - min_y, 0);
  return Rect(min_x, min_y, width, height);
}

Rect ToFlooredRectDeprecated(const RectF& rect) {
  return Rect(ToFlooredInt(rect.origin().x()),
              ToFlooredInt(rect.origin().y()),
              ToFlooredInt(rect.size().width()),
              ToFlooredInt(rect.size().height()));
}

}  // namespace gfx


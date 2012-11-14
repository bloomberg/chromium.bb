// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rect_conversions.h"

#include <cmath>

#include "base/logging.h"
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

Rect ToNearestRect(const RectF& rect) {
  float float_min_x = rect.origin().x();
  float float_min_y = rect.origin().y();
  float float_max_x = float_min_x + rect.size().width();
  float float_max_y = float_min_y + rect.size().height();

  int min_x = ToRoundedInt(float_min_x);
  int min_y = ToRoundedInt(float_min_y);
  int max_x = ToRoundedInt(float_max_x);
  int max_y = ToRoundedInt(float_max_y);

  // If these DCHECKs fail, you're using the wrong method, consider using
  // ToEnclosingRect or ToEnclosedRect instead.
  DCHECK(std::abs(min_x - float_min_x) < 0.01f);
  DCHECK(std::abs(min_y - float_min_y) < 0.01f);
  DCHECK(std::abs(max_x - float_max_x) < 0.01f);
  DCHECK(std::abs(max_y - float_max_y) < 0.01f);

  return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
}

Rect ToFlooredRectDeprecated(const RectF& rect) {
  return Rect(ToFlooredInt(rect.origin().x()),
              ToFlooredInt(rect.origin().y()),
              ToFlooredInt(rect.size().width()),
              ToFlooredInt(rect.size().height()));
}

}  // namespace gfx


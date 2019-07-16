// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include <math.h>

#include "base/logging.h"
#include "ppapi/cpp/point.h"

namespace chrome_pdf {
namespace draw_utils {

void ExpandDocumentSize(const pp::Size& rect_size, pp::Size* doc_size) {
  int width_diff = std::max(0, rect_size.width() - doc_size->width());
  doc_size->Enlarge(width_diff, rect_size.height());
}

pp::Rect GetScreenRect(const pp::Rect& rect,
                       const pp::Point& position,
                       double zoom) {
  DCHECK_GT(zoom, 0);

  int x = static_cast<int>(rect.x() * zoom - position.x());
  int y = static_cast<int>(rect.y() * zoom - position.y());
  int right = static_cast<int>(ceil(rect.right() * zoom - position.x()));
  int bottom = static_cast<int>(ceil(rect.bottom() * zoom - position.y()));
  return pp::Rect(x, y, right - x, bottom - y);
}

}  // namespace draw_utils
}  // namespace chrome_pdf

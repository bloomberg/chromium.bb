// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/path_x11.h"

#include <X11/Xutil.h>

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/path.h"

namespace gfx {

Region CreateRegionFromSkPath(const SkPath& path) {
  int point_count = path.getPoints(NULL, 0);
  scoped_ptr<SkPoint[]> points(new SkPoint[point_count]);
  path.getPoints(points.get(), point_count);
  scoped_ptr<XPoint[]> x11_points(new XPoint[point_count]);
  for (int i = 0; i < point_count; ++i) {
    x11_points[i].x = SkScalarRound(points[i].fX);
    x11_points[i].y = SkScalarRound(points[i].fY);
  }

  return XPolygonRegion(x11_points.get(), point_count, EvenOddRule);
}

}  // namespace gfx

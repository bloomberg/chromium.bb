// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/box_f.h"

#include "base/strings/stringprintf.h"

namespace gfx {

std::string BoxF::ToString() const {
  return base::StringPrintf("%s %fx%fx%f",
                            origin().ToString().c_str(),
                            width_,
                            height_,
                            depth_);
}

bool BoxF::IsEmpty() const {
  return (width_ == 0 && height_ == 0) ||
         (width_ == 0 && depth_ == 0) ||
         (height_ == 0 && depth_ == 0);
}

void BoxF::Union(const BoxF& box) {
  if (IsEmpty()) {
    *this = box;
    return;
  }
  if (box.IsEmpty())
    return;

  float min_x = std::min(x(), box.x());
  float min_y = std::min(y(), box.y());
  float min_z = std::min(z(), box.z());
  float max_x = std::max(right(), box.right());
  float max_y = std::max(bottom(), box.bottom());
  float max_z = std::max(front(), box.front());

  origin_.SetPoint(min_x, min_y, min_z);
  width_ = max_x - min_x;
  height_ = max_y - min_y;
  depth_ = max_z - min_z;
}

BoxF UnionBoxes(const BoxF& a, const BoxF& b) {
  BoxF result = a;
  result.Union(b);
  return result;
}

}  // namespace gfx

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/path.h"

#include "third_party/skia/include/core/SkRegion.h"

namespace gfx {

SkRegion* Path::CreateNativeRegion() const {
  return new SkRegion;
}

// static
NativeRegion Path::IntersectRegions(NativeRegion r1, NativeRegion r2) {
  SkRegion* new_region = new SkRegion;
  new_region->op(*r1, *r2, SkRegion::kIntersect_Op);
  return new_region;
}

// static
NativeRegion Path::CombineRegions(NativeRegion r1, NativeRegion r2) {
  SkRegion* new_region = new SkRegion;
  new_region->op(*r1, *r2, SkRegion::kUnion_Op);
  return new_region;
}

// static
NativeRegion Path::SubtractRegion(NativeRegion r1, NativeRegion r2) {
  SkRegion* new_region = new SkRegion;
  new_region->op(*r1, *r2, SkRegion::kDifference_Op);
  return new_region;
}

}  // namespace gfx

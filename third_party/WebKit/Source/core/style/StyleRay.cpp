// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleRay.h"

namespace blink {

scoped_refptr<StyleRay> StyleRay::Create(float angle,
                                         RaySize size,
                                         bool contain) {
  return WTF::AdoptRef(new StyleRay(angle, size, contain));
}

StyleRay::StyleRay(float angle, RaySize size, bool contain)
    : angle_(angle), size_(size), contain_(contain) {}

bool StyleRay::operator==(const BasicShape& o) const {
  if (!IsSameType(o))
    return false;
  const StyleRay& other = ToStyleRay(o);
  return angle_ == other.angle_ && size_ == other.size_ &&
         contain_ == other.contain_;
}

void StyleRay::GetPath(Path&, const FloatRect&) {
  // ComputedStyle::ApplyMotionPathTransform cannot call GetPath
  // for rays as they may have infinite length.
  NOTREACHED();
}

scoped_refptr<BasicShape> StyleRay::Blend(const BasicShape*, double) const {
  // TODO(ericwilligers): Implement animation for offset-path.
  NOTREACHED();
  return nullptr;
}

}  // namespace blink

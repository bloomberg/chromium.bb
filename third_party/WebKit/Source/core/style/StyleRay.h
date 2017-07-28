// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleRay_h
#define StyleRay_h

#include "core/style/BasicShapes.h"

namespace blink {

class StyleRay : public BasicShape {
 public:
  enum class RaySize {
    kClosestSide,
    kClosestCorner,
    kFarthestSide,
    kFarthestCorner,
    kSides
  };

  static PassRefPtr<StyleRay> Create(float angle, RaySize, bool contain);
  virtual ~StyleRay() {}

  float Angle() const { return angle_; }
  RaySize Size() const { return size_; }
  bool Contain() const { return contain_; }

  void GetPath(Path&, const FloatRect&) override;
  PassRefPtr<BasicShape> Blend(const BasicShape*, double) const override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kStyleRayType; }

 private:
  StyleRay(float angle, RaySize, bool contain);

  float angle_;
  RaySize size_;
  bool contain_;
};

DEFINE_BASICSHAPE_TYPE_CASTS(StyleRay);

}  // namespace blink

#endif

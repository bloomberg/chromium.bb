// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rrect_f.h"

#include <iomanip>
#include <iostream>

#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace gfx {

RRectF::RRectF() = default;

RRectF::RRectF(const SkRRect& rect) : skrrect_(rect) {}

RRectF::RRectF(const RRectF& rect) = default;

RRectF::RRectF(const gfx::RectF& rect)
    : skrrect_(SkRRect::MakeRect(gfx::RectFToSkRect(rect))) {}

RRectF::RRectF(float x, float y, float width, float height, float radius)
    : skrrect_(SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, width, height),
                                   radius,
                                   radius)) {}

RRectF::RRectF(float x,
               float y,
               float width,
               float height,
               float x_rad,
               float y_rad)
    : skrrect_(SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, width, height),
                                   x_rad,
                                   y_rad)) {}

RRectF::~RRectF() = default;

gfx::Vector2dF RRectF::GetSimpleRadii() const {
  DCHECK(type() <= Type::kOval);
  SkPoint result = skrrect_.getSimpleRadii();
  return gfx::Vector2dF(result.x(), result.y());
}
float RRectF::GetSimpleRadius() const {
  DCHECK(type() <= Type::kSingle);
  SkPoint result = skrrect_.getSimpleRadii();
  return result.x();
}

RRectF::Type RRectF::type() const {
  SkPoint rad;
  switch (skrrect_.getType()) {
    case SkRRect::kEmpty_Type:
      return Type::kEmpty;
    case SkRRect::kRect_Type:
      return Type::kRect;
    case SkRRect::kSimple_Type:
      rad = skrrect_.getSimpleRadii();
      if (rad.x() == rad.y()) {
        return Type::kSingle;
      }
      return Type::kSimple;
    case SkRRect::kOval_Type:
      return Type::kOval;
    case SkRRect::kNinePatch_Type:
    case SkRRect::kComplex_Type:
    default:
      return Type::kComplex;
  };
}

gfx::Vector2dF RRectF::GetCornerRadii(Corner corner) const {
  SkPoint result = skrrect_.radii(SkRRect::Corner(corner));
  return gfx::Vector2dF(result.x(), result.y());
}

void RRectF::GetAllRadii(SkVector radii[4]) const {
  // Unfortunately, the only way to get all radii is one at a time.
  radii[SkRRect::kUpperLeft_Corner] =
      skrrect_.radii(SkRRect::kUpperLeft_Corner);
  radii[SkRRect::kUpperRight_Corner] =
      skrrect_.radii(SkRRect::kUpperRight_Corner);
  radii[SkRRect::kLowerRight_Corner] =
      skrrect_.radii(SkRRect::kLowerRight_Corner);
  radii[SkRRect::kLowerLeft_Corner] =
      skrrect_.radii(SkRRect::kLowerLeft_Corner);
}
void RRectF::SetCornerRadii(Corner corner, const gfx::Vector2dF& radius) {
  // Unfortunately, the only way to set this is to create a new SkRRect.
  SkVector radii[4];
  GetAllRadii(radii);
  radii[SkRRect::Corner(corner)] = SkPoint::Make(radius.x(), radius.y());
  skrrect_.setRectRadii(skrrect_.rect(), radii);
}

void RRectF::Scale(float x_scale, float y_scale) {
  SkMatrix scale = SkMatrix::MakeScale(x_scale, y_scale);
  SkRRect result;
  bool success = skrrect_.transform(scale, &result);
  DCHECK(success);
  skrrect_ = result;
}

const RRectF& RRectF::operator=(const RRectF& rect) {
  skrrect_ = rect.skrrect_;
  return *this;
}

void RRectF::Offset(float horizontal, float vertical) {
  skrrect_.offset(horizontal, vertical);
}
const RRectF& RRectF::operator+=(const gfx::Vector2dF& offset) {
  Offset(offset.x(), offset.y());
  return *this;
}
const RRectF& RRectF::operator-=(const gfx::Vector2dF& offset) {
  Offset(-offset.x(), -offset.y());
  return *this;
}

std::string RRectF::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3);
  ss << rect().origin().x() << "," << rect().origin().y() << " "
     << rect().size().width() << "x" << rect().size().height();
  Type type = this->type();
  if (type <= Type::kRect) {
    ss << ", rectangular";
  } else if (type <= Type::kSingle) {
    ss << ", radius " << GetSimpleRadius();
  } else if (type <= Type::kSimple) {
    gfx::Vector2dF radii = GetSimpleRadii();
    ss << ", x_rad " << radii.x() << ", y_rad " << radii.y();
  } else {
    ss << ",";
    const Corner corners[] = {Corner::kUpperLeft, Corner::kUpperRight,
                              Corner::kLowerRight, Corner::kLowerLeft};
    for (const auto& c : corners) {
      auto this_corner = GetCornerRadii(c);
      ss << " [" << this_corner.x() << " " << this_corner.y() << "]";
    }
  }
  return ss.str();
}

}  // namespace gfx

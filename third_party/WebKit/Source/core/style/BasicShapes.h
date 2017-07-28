/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef BasicShapes_h
#define BasicShapes_h

#include "core/CoreExport.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Length.h"
#include "platform/LengthSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class FloatRect;
class FloatSize;
class Path;

class CORE_EXPORT BasicShape : public RefCounted<BasicShape> {
 public:
  virtual ~BasicShape() {}

  enum ShapeType {
    kBasicShapeEllipseType,
    kBasicShapePolygonType,
    kBasicShapeCircleType,
    kBasicShapeInsetType,
    kStyleRayType,
    kStylePathType
  };

  bool CanBlend(const BasicShape*) const;
  bool IsSameType(const BasicShape& other) const {
    return GetType() == other.GetType();
  }

  virtual void GetPath(Path&, const FloatRect&) = 0;
  virtual WindRule GetWindRule() const { return RULE_NONZERO; }
  virtual PassRefPtr<BasicShape> Blend(const BasicShape*, double) const = 0;
  virtual bool operator==(const BasicShape&) const = 0;

  virtual ShapeType GetType() const = 0;

 protected:
  BasicShape() {}
};

#define DEFINE_BASICSHAPE_TYPE_CASTS(thisType)                         \
  DEFINE_TYPE_CASTS(thisType, BasicShape, value,                       \
                    value->GetType() == BasicShape::k##thisType##Type, \
                    value.GetType() == BasicShape::k##thisType##Type)

class BasicShapeCenterCoordinate {
  DISALLOW_NEW();

 public:
  enum Direction { kTopLeft, kBottomRight };

  BasicShapeCenterCoordinate(Direction direction = kTopLeft,
                             const Length& length = Length(0, kFixed))
      : direction_(direction),
        length_(length),
        computed_length_(direction == kTopLeft
                             ? length
                             : length.SubtractFromOneHundredPercent()) {}

  BasicShapeCenterCoordinate(const BasicShapeCenterCoordinate& other)
      : direction_(other.GetDirection()),
        length_(other.length()),
        computed_length_(other.computed_length_) {}

  bool operator==(const BasicShapeCenterCoordinate& other) const {
    return direction_ == other.direction_ && length_ == other.length_ &&
           computed_length_ == other.computed_length_;
  }

  Direction GetDirection() const { return direction_; }
  const Length& length() const { return length_; }
  const Length& ComputedLength() const { return computed_length_; }

  BasicShapeCenterCoordinate Blend(const BasicShapeCenterCoordinate& other,
                                   double progress) const {
    return BasicShapeCenterCoordinate(
        kTopLeft, computed_length_.Blend(other.computed_length_, progress,
                                         kValueRangeAll));
  }

 private:
  Direction direction_;
  Length length_;
  Length computed_length_;
};

class BasicShapeRadius {
  DISALLOW_NEW();

 public:
  enum RadiusType { kValue, kClosestSide, kFarthestSide };
  BasicShapeRadius() : type_(kClosestSide) {}
  explicit BasicShapeRadius(const Length& v) : value_(v), type_(kValue) {}
  explicit BasicShapeRadius(RadiusType t) : type_(t) {}
  BasicShapeRadius(const BasicShapeRadius& other)
      : value_(other.Value()), type_(other.GetType()) {}
  bool operator==(const BasicShapeRadius& other) const {
    return type_ == other.type_ && value_ == other.value_;
  }

  const Length& Value() const { return value_; }
  RadiusType GetType() const { return type_; }

  bool CanBlend(const BasicShapeRadius& other) const {
    // FIXME determine how to interpolate between keywords. See issue 330248.
    return type_ == kValue && other.GetType() == kValue;
  }

  BasicShapeRadius Blend(const BasicShapeRadius& other, double progress) const {
    if (type_ != kValue || other.GetType() != kValue)
      return BasicShapeRadius(other);

    return BasicShapeRadius(
        value_.Blend(other.Value(), progress, kValueRangeNonNegative));
  }

 private:
  Length value_;
  RadiusType type_;
};

class CORE_EXPORT BasicShapeCircle final : public BasicShape {
 public:
  static PassRefPtr<BasicShapeCircle> Create() {
    return AdoptRef(new BasicShapeCircle);
  }

  const BasicShapeCenterCoordinate& CenterX() const { return center_x_; }
  const BasicShapeCenterCoordinate& CenterY() const { return center_y_; }
  const BasicShapeRadius& Radius() const { return radius_; }

  float FloatValueForRadiusInBox(FloatSize) const;
  void SetCenterX(BasicShapeCenterCoordinate center_x) { center_x_ = center_x; }
  void SetCenterY(BasicShapeCenterCoordinate center_y) { center_y_ = center_y; }
  void SetRadius(BasicShapeRadius radius) { radius_ = radius; }

  void GetPath(Path&, const FloatRect&) override;
  PassRefPtr<BasicShape> Blend(const BasicShape*, double) const override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kBasicShapeCircleType; }

 private:
  BasicShapeCircle() {}

  BasicShapeCenterCoordinate center_x_;
  BasicShapeCenterCoordinate center_y_;
  BasicShapeRadius radius_;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeCircle);

class BasicShapeEllipse final : public BasicShape {
 public:
  static PassRefPtr<BasicShapeEllipse> Create() {
    return AdoptRef(new BasicShapeEllipse);
  }

  const BasicShapeCenterCoordinate& CenterX() const { return center_x_; }
  const BasicShapeCenterCoordinate& CenterY() const { return center_y_; }
  const BasicShapeRadius& RadiusX() const { return radius_x_; }
  const BasicShapeRadius& RadiusY() const { return radius_y_; }
  float FloatValueForRadiusInBox(const BasicShapeRadius&,
                                 float center,
                                 float box_width_or_height) const;

  void SetCenterX(BasicShapeCenterCoordinate center_x) { center_x_ = center_x; }
  void SetCenterY(BasicShapeCenterCoordinate center_y) { center_y_ = center_y; }
  void SetRadiusX(BasicShapeRadius radius_x) { radius_x_ = radius_x; }
  void SetRadiusY(BasicShapeRadius radius_y) { radius_y_ = radius_y; }

  void GetPath(Path&, const FloatRect&) override;
  PassRefPtr<BasicShape> Blend(const BasicShape*, double) const override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kBasicShapeEllipseType; }

 private:
  BasicShapeEllipse() {}

  BasicShapeCenterCoordinate center_x_;
  BasicShapeCenterCoordinate center_y_;
  BasicShapeRadius radius_x_;
  BasicShapeRadius radius_y_;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeEllipse);

class BasicShapePolygon final : public BasicShape {
 public:
  static PassRefPtr<BasicShapePolygon> Create() {
    return AdoptRef(new BasicShapePolygon);
  }

  const Vector<Length>& Values() const { return values_; }
  Length GetXAt(unsigned i) const { return values_.at(2 * i); }
  Length GetYAt(unsigned i) const { return values_.at(2 * i + 1); }

  void SetWindRule(WindRule wind_rule) { wind_rule_ = wind_rule; }
  void AppendPoint(const Length& x, const Length& y) {
    values_.push_back(x);
    values_.push_back(y);
  }

  void GetPath(Path&, const FloatRect&) override;
  PassRefPtr<BasicShape> Blend(const BasicShape*, double) const override;
  bool operator==(const BasicShape&) const override;

  WindRule GetWindRule() const override { return wind_rule_; }

  ShapeType GetType() const override { return kBasicShapePolygonType; }

 private:
  BasicShapePolygon() : wind_rule_(RULE_NONZERO) {}

  WindRule wind_rule_;
  Vector<Length> values_;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapePolygon);

class BasicShapeInset : public BasicShape {
 public:
  static PassRefPtr<BasicShapeInset> Create() {
    return AdoptRef(new BasicShapeInset);
  }

  const Length& Top() const { return top_; }
  const Length& Right() const { return right_; }
  const Length& Bottom() const { return bottom_; }
  const Length& Left() const { return left_; }

  const LengthSize& TopLeftRadius() const { return top_left_radius_; }
  const LengthSize& TopRightRadius() const { return top_right_radius_; }
  const LengthSize& BottomRightRadius() const { return bottom_right_radius_; }
  const LengthSize& BottomLeftRadius() const { return bottom_left_radius_; }

  void SetTop(const Length& top) { top_ = top; }
  void SetRight(const Length& right) { right_ = right; }
  void SetBottom(const Length& bottom) { bottom_ = bottom; }
  void SetLeft(const Length& left) { left_ = left; }

  void SetTopLeftRadius(const LengthSize& radius) { top_left_radius_ = radius; }
  void SetTopRightRadius(const LengthSize& radius) {
    top_right_radius_ = radius;
  }
  void SetBottomRightRadius(const LengthSize& radius) {
    bottom_right_radius_ = radius;
  }
  void SetBottomLeftRadius(const LengthSize& radius) {
    bottom_left_radius_ = radius;
  }

  void GetPath(Path&, const FloatRect&) override;
  PassRefPtr<BasicShape> Blend(const BasicShape*, double) const override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kBasicShapeInsetType; }

 private:
  BasicShapeInset() {}

  Length right_;
  Length top_;
  Length bottom_;
  Length left_;

  LengthSize top_left_radius_;
  LengthSize top_right_radius_;
  LengthSize bottom_right_radius_;
  LengthSize bottom_left_radius_;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeInset);

}  // namespace blink
#endif

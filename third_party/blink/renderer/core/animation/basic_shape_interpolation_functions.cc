// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/length_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"

namespace blink {

class BasicShapeNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<NonInterpolableValue> Create(
      BasicShape::ShapeType type) {
    return base::AdoptRef(new BasicShapeNonInterpolableValue(type));
  }
  static scoped_refptr<NonInterpolableValue> CreatePolygon(WindRule wind_rule,
                                                           wtf_size_t size) {
    return base::AdoptRef(new BasicShapeNonInterpolableValue(wind_rule, size));
  }

  BasicShape::ShapeType GetShapeType() const { return type_; }

  WindRule GetWindRule() const {
    DCHECK_EQ(GetShapeType(), BasicShape::kBasicShapePolygonType);
    return wind_rule_;
  }
  wtf_size_t size() const {
    DCHECK_EQ(GetShapeType(), BasicShape::kBasicShapePolygonType);
    return size_;
  }

  bool IsCompatibleWith(const BasicShapeNonInterpolableValue& other) const {
    if (GetShapeType() != other.GetShapeType())
      return false;
    switch (GetShapeType()) {
      case BasicShape::kBasicShapeCircleType:
      case BasicShape::kBasicShapeEllipseType:
      case BasicShape::kBasicShapeInsetType:
        return true;
      case BasicShape::kBasicShapePolygonType:
        return GetWindRule() == other.GetWindRule() && size() == other.size();
      default:
        NOTREACHED();
        return false;
    }
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  BasicShapeNonInterpolableValue(BasicShape::ShapeType type)
      : type_(type), wind_rule_(RULE_NONZERO), size_(0) {
    DCHECK_NE(type, BasicShape::kBasicShapePolygonType);
  }
  BasicShapeNonInterpolableValue(WindRule wind_rule, wtf_size_t size)
      : type_(BasicShape::kBasicShapePolygonType),
        wind_rule_(wind_rule),
        size_(size) {}

  const BasicShape::ShapeType type_;
  const WindRule wind_rule_;
  const wtf_size_t size_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(BasicShapeNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(BasicShapeNonInterpolableValue);

namespace {

std::unique_ptr<InterpolableValue> Unwrap(InterpolationValue&& value) {
  DCHECK(value.interpolable_value);
  return std::move(value.interpolable_value);
}

std::unique_ptr<InterpolableValue> ConvertCSSCoordinate(
    const CSSValue* coordinate) {
  if (coordinate)
    return Unwrap(
        CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(
            *coordinate));
  return Unwrap(
      LengthInterpolationFunctions::MaybeConvertLength(Length::Percent(50), 1));
}

std::unique_ptr<InterpolableValue> ConvertCoordinate(
    const BasicShapeCenterCoordinate& coordinate,
    double zoom) {
  return Unwrap(LengthInterpolationFunctions::MaybeConvertLength(
      coordinate.ComputedLength(), zoom));
}

std::unique_ptr<InterpolableValue> CreateNeutralInterpolableCoordinate() {
  return LengthInterpolationFunctions::CreateNeutralInterpolableValue();
}

BasicShapeCenterCoordinate CreateCoordinate(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  return BasicShapeCenterCoordinate(
      BasicShapeCenterCoordinate::kTopLeft,
      LengthInterpolationFunctions::CreateLength(
          interpolable_value, nullptr, conversion_data, kValueRangeAll));
}

std::unique_ptr<InterpolableValue> ConvertCSSRadius(const CSSValue* radius) {
  if (!radius || radius->IsIdentifierValue())
    return nullptr;
  return Unwrap(LengthInterpolationFunctions::MaybeConvertCSSValue(*radius));
}

std::unique_ptr<InterpolableValue> ConvertRadius(const BasicShapeRadius& radius,
                                                 double zoom) {
  if (radius.GetType() != BasicShapeRadius::kValue)
    return nullptr;
  return Unwrap(
      LengthInterpolationFunctions::MaybeConvertLength(radius.Value(), zoom));
}

std::unique_ptr<InterpolableValue> CreateNeutralInterpolableRadius() {
  return LengthInterpolationFunctions::CreateNeutralInterpolableValue();
}

BasicShapeRadius CreateRadius(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  return BasicShapeRadius(LengthInterpolationFunctions::CreateLength(
      interpolable_value, nullptr, conversion_data, kValueRangeNonNegative));
}

std::unique_ptr<InterpolableValue> ConvertCSSLength(const CSSValue* length) {
  if (!length)
    return LengthInterpolationFunctions::CreateNeutralInterpolableValue();
  return Unwrap(LengthInterpolationFunctions::MaybeConvertCSSValue(*length));
}

std::unique_ptr<InterpolableValue> ConvertLength(const Length& length,
                                                 double zoom) {
  return Unwrap(LengthInterpolationFunctions::MaybeConvertLength(length, zoom));
}

std::unique_ptr<InterpolableValue> ConvertCSSBorderRadiusWidth(
    const CSSValuePair* pair) {
  return ConvertCSSLength(pair ? &pair->First() : nullptr);
}

std::unique_ptr<InterpolableValue> ConvertCSSBorderRadiusHeight(
    const CSSValuePair* pair) {
  return ConvertCSSLength(pair ? &pair->Second() : nullptr);
}

LengthSize CreateBorderRadius(
    const InterpolableValue& width,
    const InterpolableValue& height,
    const CSSToLengthConversionData& conversion_data) {
  return LengthSize(
      LengthInterpolationFunctions::CreateLength(
          width, nullptr, conversion_data, kValueRangeNonNegative),
      LengthInterpolationFunctions::CreateLength(
          height, nullptr, conversion_data, kValueRangeNonNegative));
}

namespace circle_functions {

enum CircleComponentIndex : unsigned {
  kCircleCenterXIndex,
  kCircleCenterYIndex,
  kCircleRadiusIndex,
  kCircleComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeCircleValue& circle) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, ConvertCSSCoordinate(circle.CenterX()));
  list->Set(kCircleCenterYIndex, ConvertCSSCoordinate(circle.CenterY()));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertCSSRadius(circle.Radius())))
    return nullptr;
  list->Set(kCircleRadiusIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeCircleType));
}

InterpolationValue ConvertBasicShape(const BasicShapeCircle& circle,
                                     double zoom) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, ConvertCoordinate(circle.CenterX(), zoom));
  list->Set(kCircleCenterYIndex, ConvertCoordinate(circle.CenterY(), zoom));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertRadius(circle.Radius(), zoom)))
    return nullptr;
  list->Set(kCircleRadiusIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeCircleType));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kCircleCenterYIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kCircleRadiusIndex, CreateNeutralInterpolableRadius());
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapeCircle> circle = BasicShapeCircle::Create();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  circle->SetCenterX(
      CreateCoordinate(*list.Get(kCircleCenterXIndex), conversion_data));
  circle->SetCenterY(
      CreateCoordinate(*list.Get(kCircleCenterYIndex), conversion_data));
  circle->SetRadius(
      CreateRadius(*list.Get(kCircleRadiusIndex), conversion_data));
  return circle;
}

}  // namespace circle_functions

namespace ellipse_functions {

enum EllipseComponentIndex : unsigned {
  kEllipseCenterXIndex,
  kEllipseCenterYIndex,
  kEllipseRadiusXIndex,
  kEllipseRadiusYIndex,
  kEllipseComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeEllipseValue& ellipse) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, ConvertCSSCoordinate(ellipse.CenterX()));
  list->Set(kEllipseCenterYIndex, ConvertCSSCoordinate(ellipse.CenterY()));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertCSSRadius(ellipse.RadiusX())))
    return nullptr;
  list->Set(kEllipseRadiusXIndex, std::move(radius));
  if (!(radius = ConvertCSSRadius(ellipse.RadiusY())))
    return nullptr;
  list->Set(kEllipseRadiusYIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeEllipseType));
}

InterpolationValue ConvertBasicShape(const BasicShapeEllipse& ellipse,
                                     double zoom) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, ConvertCoordinate(ellipse.CenterX(), zoom));
  list->Set(kEllipseCenterYIndex, ConvertCoordinate(ellipse.CenterY(), zoom));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertRadius(ellipse.RadiusX(), zoom)))
    return nullptr;
  list->Set(kEllipseRadiusXIndex, std::move(radius));
  if (!(radius = ConvertRadius(ellipse.RadiusY(), zoom)))
    return nullptr;
  list->Set(kEllipseRadiusYIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeEllipseType));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kEllipseCenterYIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kEllipseRadiusXIndex, CreateNeutralInterpolableRadius());
  list->Set(kEllipseRadiusYIndex, CreateNeutralInterpolableRadius());
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapeEllipse> ellipse = BasicShapeEllipse::Create();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  ellipse->SetCenterX(
      CreateCoordinate(*list.Get(kEllipseCenterXIndex), conversion_data));
  ellipse->SetCenterY(
      CreateCoordinate(*list.Get(kEllipseCenterYIndex), conversion_data));
  ellipse->SetRadiusX(
      CreateRadius(*list.Get(kEllipseRadiusXIndex), conversion_data));
  ellipse->SetRadiusY(
      CreateRadius(*list.Get(kEllipseRadiusYIndex), conversion_data));
  return ellipse;
}

}  // namespace ellipse_functions

namespace inset_functions {

enum InsetComponentIndex : unsigned {
  kInsetTopIndex,
  kInsetRightIndex,
  kInsetBottomIndex,
  kInsetLeftIndex,
  kInsetBorderTopLeftWidthIndex,
  kInsetBorderTopLeftHeightIndex,
  kInsetBorderTopRightWidthIndex,
  kInsetBorderTopRightHeightIndex,
  kInsetBorderBottomRightWidthIndex,
  kInsetBorderBottomRightHeightIndex,
  kInsetBorderBottomLeftWidthIndex,
  kInsetBorderBottomLeftHeightIndex,
  kInsetComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeInsetValue& inset) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kInsetComponentIndexCount);
  list->Set(kInsetTopIndex, ConvertCSSLength(inset.Top()));
  list->Set(kInsetRightIndex, ConvertCSSLength(inset.Right()));
  list->Set(kInsetBottomIndex, ConvertCSSLength(inset.Bottom()));
  list->Set(kInsetLeftIndex, ConvertCSSLength(inset.Left()));

  list->Set(kInsetBorderTopLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.TopLeftRadius()));
  list->Set(kInsetBorderTopLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.TopLeftRadius()));
  list->Set(kInsetBorderTopRightWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.TopRightRadius()));
  list->Set(kInsetBorderTopRightHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.TopRightRadius()));
  list->Set(kInsetBorderBottomRightWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.BottomRightRadius()));
  list->Set(kInsetBorderBottomRightHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.BottomRightRadius()));
  list->Set(kInsetBorderBottomLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.BottomLeftRadius()));
  list->Set(kInsetBorderBottomLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.BottomLeftRadius()));
  return InterpolationValue(
      std::move(list),
      BasicShapeNonInterpolableValue::Create(BasicShape::kBasicShapeInsetType));
}

InterpolationValue ConvertBasicShape(const BasicShapeInset& inset,
                                     double zoom) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kInsetComponentIndexCount);
  list->Set(kInsetTopIndex, ConvertLength(inset.Top(), zoom));
  list->Set(kInsetRightIndex, ConvertLength(inset.Right(), zoom));
  list->Set(kInsetBottomIndex, ConvertLength(inset.Bottom(), zoom));
  list->Set(kInsetLeftIndex, ConvertLength(inset.Left(), zoom));

  list->Set(kInsetBorderTopLeftWidthIndex,
            ConvertLength(inset.TopLeftRadius().Width(), zoom));
  list->Set(kInsetBorderTopLeftHeightIndex,
            ConvertLength(inset.TopLeftRadius().Height(), zoom));
  list->Set(kInsetBorderTopRightWidthIndex,
            ConvertLength(inset.TopRightRadius().Width(), zoom));
  list->Set(kInsetBorderTopRightHeightIndex,
            ConvertLength(inset.TopRightRadius().Height(), zoom));
  list->Set(kInsetBorderBottomRightWidthIndex,
            ConvertLength(inset.BottomRightRadius().Width(), zoom));
  list->Set(kInsetBorderBottomRightHeightIndex,
            ConvertLength(inset.BottomRightRadius().Height(), zoom));
  list->Set(kInsetBorderBottomLeftWidthIndex,
            ConvertLength(inset.BottomLeftRadius().Width(), zoom));
  list->Set(kInsetBorderBottomLeftHeightIndex,
            ConvertLength(inset.BottomLeftRadius().Height(), zoom));
  return InterpolationValue(
      std::move(list),
      BasicShapeNonInterpolableValue::Create(BasicShape::kBasicShapeInsetType));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kInsetComponentIndexCount);
  list->Set(kInsetTopIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetRightIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBottomIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetLeftIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());

  list->Set(kInsetBorderTopLeftWidthIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderTopLeftHeightIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderTopRightWidthIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderTopRightHeightIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderBottomRightWidthIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderBottomRightHeightIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderBottomLeftWidthIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  list->Set(kInsetBorderBottomLeftHeightIndex,
            LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapeInset> inset = BasicShapeInset::Create();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  inset->SetTop(LengthInterpolationFunctions::CreateLength(
      *list.Get(kInsetTopIndex), nullptr, conversion_data, kValueRangeAll));
  inset->SetRight(LengthInterpolationFunctions::CreateLength(
      *list.Get(kInsetRightIndex), nullptr, conversion_data, kValueRangeAll));
  inset->SetBottom(LengthInterpolationFunctions::CreateLength(
      *list.Get(kInsetBottomIndex), nullptr, conversion_data, kValueRangeAll));
  inset->SetLeft(LengthInterpolationFunctions::CreateLength(
      *list.Get(kInsetLeftIndex), nullptr, conversion_data, kValueRangeAll));

  inset->SetTopLeftRadius(CreateBorderRadius(
      *list.Get(kInsetBorderTopLeftWidthIndex),
      *list.Get(kInsetBorderTopLeftHeightIndex), conversion_data));
  inset->SetTopRightRadius(CreateBorderRadius(
      *list.Get(kInsetBorderTopRightWidthIndex),
      *list.Get(kInsetBorderTopRightHeightIndex), conversion_data));
  inset->SetBottomRightRadius(CreateBorderRadius(
      *list.Get(kInsetBorderBottomRightWidthIndex),
      *list.Get(kInsetBorderBottomRightHeightIndex), conversion_data));
  inset->SetBottomLeftRadius(CreateBorderRadius(
      *list.Get(kInsetBorderBottomLeftWidthIndex),
      *list.Get(kInsetBorderBottomLeftHeightIndex), conversion_data));
  return inset;
}

}  // namespace inset_functions

namespace polygon_functions {

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapePolygonValue& polygon) {
  wtf_size_t size = polygon.Values().size();
  std::unique_ptr<InterpolableList> list = InterpolableList::Create(size);
  for (wtf_size_t i = 0; i < size; i++)
    list->Set(i, ConvertCSSLength(polygon.Values()[i].Get()));
  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::CreatePolygon(
                                polygon.GetWindRule(), size));
}

InterpolationValue ConvertBasicShape(const BasicShapePolygon& polygon,
                                     double zoom) {
  wtf_size_t size = polygon.Values().size();
  std::unique_ptr<InterpolableList> list = InterpolableList::Create(size);
  for (wtf_size_t i = 0; i < size; i++)
    list->Set(i, ConvertLength(polygon.Values()[i], zoom));
  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::CreatePolygon(
                                polygon.GetWindRule(), size));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue(
    const BasicShapeNonInterpolableValue& non_interpolable_value) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(non_interpolable_value.size());
  for (wtf_size_t i = 0; i < non_interpolable_value.size(); i++)
    list->Set(i,
              LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const BasicShapeNonInterpolableValue& non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapePolygon> polygon = BasicShapePolygon::Create();
  polygon->SetWindRule(non_interpolable_value.GetWindRule());
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  wtf_size_t size = non_interpolable_value.size();
  DCHECK_EQ(list.length(), size);
  DCHECK_EQ(size % 2, 0U);
  for (wtf_size_t i = 0; i < size; i += 2) {
    polygon->AppendPoint(
        LengthInterpolationFunctions::CreateLength(
            *list.Get(i), nullptr, conversion_data, kValueRangeAll),
        LengthInterpolationFunctions::CreateLength(
            *list.Get(i + 1), nullptr, conversion_data, kValueRangeAll));
  }
  return polygon;
}

}  // namespace polygon_functions

}  // namespace

InterpolationValue basic_shape_interpolation_functions::MaybeConvertCSSValue(
    const CSSValue& value) {
  if (value.IsBasicShapeCircleValue()) {
    return circle_functions::ConvertCSSValue(
        cssvalue::ToCSSBasicShapeCircleValue(value));
  }
  if (value.IsBasicShapeEllipseValue()) {
    return ellipse_functions::ConvertCSSValue(
        cssvalue::ToCSSBasicShapeEllipseValue(value));
  }
  if (value.IsBasicShapeInsetValue()) {
    return inset_functions::ConvertCSSValue(
        cssvalue::ToCSSBasicShapeInsetValue(value));
  }
  if (value.IsBasicShapePolygonValue()) {
    return polygon_functions::ConvertCSSValue(
        cssvalue::ToCSSBasicShapePolygonValue(value));
  }
  return nullptr;
}

InterpolationValue basic_shape_interpolation_functions::MaybeConvertBasicShape(
    const BasicShape* shape,
    double zoom) {
  if (!shape)
    return nullptr;
  switch (shape->GetType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::ConvertBasicShape(ToBasicShapeCircle(*shape),
                                                 zoom);
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::ConvertBasicShape(ToBasicShapeEllipse(*shape),
                                                  zoom);
    case BasicShape::kBasicShapeInsetType:
      return inset_functions::ConvertBasicShape(ToBasicShapeInset(*shape),
                                                zoom);
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::ConvertBasicShape(ToBasicShapePolygon(*shape),
                                                  zoom);
    default:
      NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<InterpolableValue>
basic_shape_interpolation_functions::CreateNeutralValue(
    const NonInterpolableValue& untyped_non_interpolable_value) {
  const BasicShapeNonInterpolableValue& non_interpolable_value =
      ToBasicShapeNonInterpolableValue(untyped_non_interpolable_value);
  switch (non_interpolable_value.GetShapeType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeInsetType:
      return inset_functions::CreateNeutralValue();
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::CreateNeutralValue(non_interpolable_value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool basic_shape_interpolation_functions::ShapesAreCompatible(
    const NonInterpolableValue& a,
    const NonInterpolableValue& b) {
  return ToBasicShapeNonInterpolableValue(a).IsCompatibleWith(
      ToBasicShapeNonInterpolableValue(b));
}

scoped_refptr<BasicShape> basic_shape_interpolation_functions::CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  const BasicShapeNonInterpolableValue& non_interpolable_value =
      ToBasicShapeNonInterpolableValue(untyped_non_interpolable_value);
  switch (non_interpolable_value.GetShapeType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::CreateBasicShape(interpolable_value,
                                                conversion_data);
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::CreateBasicShape(interpolable_value,
                                                 conversion_data);
    case BasicShape::kBasicShapeInsetType:
      return inset_functions::CreateBasicShape(interpolable_value,
                                               conversion_data);
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::CreateBasicShape(
          interpolable_value, non_interpolable_value, conversion_data);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink

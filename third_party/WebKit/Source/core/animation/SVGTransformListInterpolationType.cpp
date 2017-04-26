// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGTransformListInterpolationType.h"

#include <memory>
#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/SVGTransformList.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class SVGTransformNonInterpolableValue : public NonInterpolableValue {
 public:
  virtual ~SVGTransformNonInterpolableValue() {}

  static PassRefPtr<SVGTransformNonInterpolableValue> Create(
      Vector<SVGTransformType>& transform_types) {
    return AdoptRef(new SVGTransformNonInterpolableValue(transform_types));
  }

  const Vector<SVGTransformType>& TransformTypes() const {
    return transform_types_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGTransformNonInterpolableValue(Vector<SVGTransformType>& transform_types) {
    transform_types_.swap(transform_types);
  }

  Vector<SVGTransformType> transform_types_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGTransformNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGTransformNonInterpolableValue);

namespace {

std::unique_ptr<InterpolableValue> TranslateToInterpolableValue(
    SVGTransform* transform) {
  FloatPoint translate = transform->Translate();
  std::unique_ptr<InterpolableList> result = InterpolableList::Create(2);
  result->Set(0, InterpolableNumber::Create(translate.X()));
  result->Set(1, InterpolableNumber::Create(translate.Y()));
  return std::move(result);
}

SVGTransform* TranslateFromInterpolableValue(const InterpolableValue& value) {
  const InterpolableList& list = ToInterpolableList(value);

  SVGTransform* transform = SVGTransform::Create(kSvgTransformTranslate);
  transform->SetTranslate(ToInterpolableNumber(list.Get(0))->Value(),
                          ToInterpolableNumber(list.Get(1))->Value());
  return transform;
}

std::unique_ptr<InterpolableValue> ScaleToInterpolableValue(
    SVGTransform* transform) {
  FloatSize scale = transform->Scale();
  std::unique_ptr<InterpolableList> result = InterpolableList::Create(2);
  result->Set(0, InterpolableNumber::Create(scale.Width()));
  result->Set(1, InterpolableNumber::Create(scale.Height()));
  return std::move(result);
}

SVGTransform* ScaleFromInterpolableValue(const InterpolableValue& value) {
  const InterpolableList& list = ToInterpolableList(value);

  SVGTransform* transform = SVGTransform::Create(kSvgTransformScale);
  transform->SetScale(ToInterpolableNumber(list.Get(0))->Value(),
                      ToInterpolableNumber(list.Get(1))->Value());
  return transform;
}

std::unique_ptr<InterpolableValue> RotateToInterpolableValue(
    SVGTransform* transform) {
  FloatPoint rotation_center = transform->RotationCenter();
  std::unique_ptr<InterpolableList> result = InterpolableList::Create(3);
  result->Set(0, InterpolableNumber::Create(transform->Angle()));
  result->Set(1, InterpolableNumber::Create(rotation_center.X()));
  result->Set(2, InterpolableNumber::Create(rotation_center.Y()));
  return std::move(result);
}

SVGTransform* RotateFromInterpolableValue(const InterpolableValue& value) {
  const InterpolableList& list = ToInterpolableList(value);

  SVGTransform* transform = SVGTransform::Create(kSvgTransformRotate);
  transform->SetRotate(ToInterpolableNumber(list.Get(0))->Value(),
                       ToInterpolableNumber(list.Get(1))->Value(),
                       ToInterpolableNumber(list.Get(2))->Value());
  return transform;
}

std::unique_ptr<InterpolableValue> SkewXToInterpolableValue(
    SVGTransform* transform) {
  return InterpolableNumber::Create(transform->Angle());
}

SVGTransform* SkewXFromInterpolableValue(const InterpolableValue& value) {
  SVGTransform* transform = SVGTransform::Create(kSvgTransformSkewx);
  transform->SetSkewX(ToInterpolableNumber(value).Value());
  return transform;
}

std::unique_ptr<InterpolableValue> SkewYToInterpolableValue(
    SVGTransform* transform) {
  return InterpolableNumber::Create(transform->Angle());
}

SVGTransform* SkewYFromInterpolableValue(const InterpolableValue& value) {
  SVGTransform* transform = SVGTransform::Create(kSvgTransformSkewy);
  transform->SetSkewY(ToInterpolableNumber(value).Value());
  return transform;
}

std::unique_ptr<InterpolableValue> ToInterpolableValue(
    SVGTransform* transform,
    SVGTransformType transform_type) {
  switch (transform_type) {
    case kSvgTransformTranslate:
      return TranslateToInterpolableValue(transform);
    case kSvgTransformScale:
      return ScaleToInterpolableValue(transform);
    case kSvgTransformRotate:
      return RotateToInterpolableValue(transform);
    case kSvgTransformSkewx:
      return SkewXToInterpolableValue(transform);
    case kSvgTransformSkewy:
      return SkewYToInterpolableValue(transform);
    case kSvgTransformMatrix:
    case kSvgTransformUnknown:
      NOTREACHED();
  }
  NOTREACHED();
  return nullptr;
}

SVGTransform* FromInterpolableValue(const InterpolableValue& value,
                                    SVGTransformType transform_type) {
  switch (transform_type) {
    case kSvgTransformTranslate:
      return TranslateFromInterpolableValue(value);
    case kSvgTransformScale:
      return ScaleFromInterpolableValue(value);
    case kSvgTransformRotate:
      return RotateFromInterpolableValue(value);
    case kSvgTransformSkewx:
      return SkewXFromInterpolableValue(value);
    case kSvgTransformSkewy:
      return SkewYFromInterpolableValue(value);
    case kSvgTransformMatrix:
    case kSvgTransformUnknown:
      NOTREACHED();
  }
  NOTREACHED();
  return nullptr;
}

const Vector<SVGTransformType>& GetTransformTypes(
    const InterpolationValue& value) {
  return ToSVGTransformNonInterpolableValue(*value.non_interpolable_value)
      .TransformTypes();
}

class SVGTransformListChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<SVGTransformListChecker> Create(
      const InterpolationValue& underlying) {
    return WTF::WrapUnique(new SVGTransformListChecker(underlying));
  }

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    // TODO(suzyh): change maybeConvertSingle so we don't have to recalculate
    // for changes to the interpolable values
    if (!underlying && !underlying_)
      return true;
    if (!underlying || !underlying_)
      return false;
    return underlying_.interpolable_value->Equals(
               *underlying.interpolable_value) &&
           GetTransformTypes(underlying_) == GetTransformTypes(underlying);
  }

 private:
  SVGTransformListChecker(const InterpolationValue& underlying)
      : underlying_(underlying.Clone()) {}

  const InterpolationValue underlying_;
};

}  // namespace

InterpolationValue SVGTransformListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  NOTREACHED();
  // This function is no longer called, because maybeConvertSingle has been
  // overridden.
  return nullptr;
}

InterpolationValue SVGTransformListInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedTransformList)
    return nullptr;

  const SVGTransformList& svg_list = ToSVGTransformList(svg_value);
  std::unique_ptr<InterpolableList> result =
      InterpolableList::Create(svg_list.length());

  Vector<SVGTransformType> transform_types;
  for (size_t i = 0; i < svg_list.length(); i++) {
    const SVGTransform* transform = svg_list.at(i);
    SVGTransformType transform_type(transform->TransformType());
    if (transform_type == kSvgTransformMatrix) {
      // TODO(ericwilligers): Support matrix interpolation.
      return nullptr;
    }
    result->Set(i, ToInterpolableValue(transform->Clone(), transform_type));
    transform_types.push_back(transform_type);
  }
  return InterpolationValue(
      std::move(result),
      SVGTransformNonInterpolableValue::Create(transform_types));
}

InterpolationValue SVGTransformListInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  Vector<SVGTransformType> types;
  Vector<std::unique_ptr<InterpolableValue>> interpolable_parts;

  if (keyframe.Composite() == EffectModel::kCompositeAdd) {
    if (underlying) {
      types.AppendVector(GetTransformTypes(underlying));
      interpolable_parts.push_back(underlying.interpolable_value->Clone());
    }
    conversion_checkers.push_back(SVGTransformListChecker::Create(underlying));
  } else {
    DCHECK(!keyframe.IsNeutral());
  }

  if (!keyframe.IsNeutral()) {
    SVGPropertyBase* svg_value = environment.SvgBaseValue().CloneForAnimation(
        ToSVGPropertySpecificKeyframe(keyframe).Value());
    InterpolationValue value = MaybeConvertSVGValue(*svg_value);
    if (!value)
      return nullptr;
    types.AppendVector(GetTransformTypes(value));
    interpolable_parts.push_back(std::move(value.interpolable_value));
  }

  std::unique_ptr<InterpolableList> interpolable_list =
      InterpolableList::Create(types.size());
  size_t interpolable_list_index = 0;
  for (auto& part : interpolable_parts) {
    InterpolableList& list = ToInterpolableList(*part);
    for (size_t i = 0; i < list.length(); ++i) {
      interpolable_list->Set(interpolable_list_index,
                             std::move(list.GetMutable(i)));
      ++interpolable_list_index;
    }
  }

  return InterpolationValue(std::move(interpolable_list),
                            SVGTransformNonInterpolableValue::Create(types));
}

SVGPropertyBase* SVGTransformListInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) const {
  SVGTransformList* result = SVGTransformList::Create();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  const Vector<SVGTransformType>& transform_types =
      ToSVGTransformNonInterpolableValue(non_interpolable_value)
          ->TransformTypes();
  for (size_t i = 0; i < list.length(); ++i)
    result->Append(FromInterpolableValue(*list.Get(i), transform_types.at(i)));
  return result;
}

PairwiseInterpolationValue SVGTransformListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (GetTransformTypes(start) != GetTransformTypes(end))
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(end.non_interpolable_value));
}

void SVGTransformListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

}  // namespace blink

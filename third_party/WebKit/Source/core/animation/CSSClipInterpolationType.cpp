// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSClipInterpolationType.h"

#include <memory>
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

struct ClipAutos {
  ClipAutos()
      : is_auto(true),
        is_top_auto(false),
        is_right_auto(false),
        is_bottom_auto(false),
        is_left_auto(false) {}
  ClipAutos(bool is_top_auto,
            bool is_right_auto,
            bool is_bottom_auto,
            bool is_left_auto)
      : is_auto(false),
        is_top_auto(is_top_auto),
        is_right_auto(is_right_auto),
        is_bottom_auto(is_bottom_auto),
        is_left_auto(is_left_auto) {}
  explicit ClipAutos(const LengthBox& clip)
      : is_auto(false),
        is_top_auto(clip.Top().IsAuto()),
        is_right_auto(clip.Right().IsAuto()),
        is_bottom_auto(clip.Bottom().IsAuto()),
        is_left_auto(clip.Left().IsAuto()) {}

  bool operator==(const ClipAutos& other) const {
    return is_auto == other.is_auto && is_top_auto == other.is_top_auto &&
           is_right_auto == other.is_right_auto &&
           is_bottom_auto == other.is_bottom_auto &&
           is_left_auto == other.is_left_auto;
  }
  bool operator!=(const ClipAutos& other) const { return !(*this == other); }

  bool is_auto;
  bool is_top_auto;
  bool is_right_auto;
  bool is_bottom_auto;
  bool is_left_auto;
};

static ClipAutos GetClipAutos(const ComputedStyle& style) {
  if (style.HasAutoClip())
    return ClipAutos();
  return ClipAutos(style.ClipTop().IsAuto(), style.ClipRight().IsAuto(),
                   style.ClipBottom().IsAuto(), style.ClipLeft().IsAuto());
}

class InheritedAutosChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedAutosChecker> Create(
      const ClipAutos& inherited_autos) {
    return WTF::WrapUnique(new InheritedAutosChecker(inherited_autos));
  }

 private:
  InheritedAutosChecker(const ClipAutos& inherited_autos)
      : inherited_autos_(inherited_autos) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_autos_ == GetClipAutos(*state.ParentStyle());
  }

  const ClipAutos inherited_autos_;
};

class CSSClipNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSClipNonInterpolableValue() final {}

  static RefPtr<CSSClipNonInterpolableValue> Create(
      const ClipAutos& clip_autos) {
    return WTF::AdoptRef(new CSSClipNonInterpolableValue(clip_autos));
  }

  const ClipAutos& GetClipAutos() const { return clip_autos_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSClipNonInterpolableValue(const ClipAutos& clip_autos)
      : clip_autos_(clip_autos) {
    DCHECK(!clip_autos_.is_auto);
  }

  const ClipAutos clip_autos_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSClipNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSClipNonInterpolableValue);

class UnderlyingAutosChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  ~UnderlyingAutosChecker() final {}

  static std::unique_ptr<UnderlyingAutosChecker> Create(
      const ClipAutos& underlying_autos) {
    return WTF::WrapUnique(new UnderlyingAutosChecker(underlying_autos));
  }

  static ClipAutos GetUnderlyingAutos(const InterpolationValue& underlying) {
    if (!underlying)
      return ClipAutos();
    return ToCSSClipNonInterpolableValue(*underlying.non_interpolable_value)
        .GetClipAutos();
  }

 private:
  UnderlyingAutosChecker(const ClipAutos& underlying_autos)
      : underlying_autos_(underlying_autos) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_autos_ == GetUnderlyingAutos(underlying);
  }

  const ClipAutos underlying_autos_;
};

enum ClipComponentIndex : unsigned {
  kClipTop,
  kClipRight,
  kClipBottom,
  kClipLeft,
  kClipComponentIndexCount,
};

static std::unique_ptr<InterpolableValue> ConvertClipComponent(
    const Length& length,
    double zoom) {
  if (length.IsAuto())
    return InterpolableList::Create(0);
  return LengthInterpolationFunctions::MaybeConvertLength(length, zoom)
      .interpolable_value;
}

static InterpolationValue CreateClipValue(const LengthBox& clip, double zoom) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kClipComponentIndexCount);
  list->Set(kClipTop, ConvertClipComponent(clip.Top(), zoom));
  list->Set(kClipRight, ConvertClipComponent(clip.Right(), zoom));
  list->Set(kClipBottom, ConvertClipComponent(clip.Bottom(), zoom));
  list->Set(kClipLeft, ConvertClipComponent(clip.Left(), zoom));
  return InterpolationValue(
      std::move(list), CSSClipNonInterpolableValue::Create(ClipAutos(clip)));
}

InterpolationValue CSSClipInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  ClipAutos underlying_autos =
      UnderlyingAutosChecker::GetUnderlyingAutos(underlying);
  conversion_checkers.push_back(
      UnderlyingAutosChecker::Create(underlying_autos));
  if (underlying_autos.is_auto)
    return nullptr;
  LengthBox neutral_box(
      underlying_autos.is_top_auto ? Length(kAuto) : Length(0, kFixed),
      underlying_autos.is_right_auto ? Length(kAuto) : Length(0, kFixed),
      underlying_autos.is_bottom_auto ? Length(kAuto) : Length(0, kFixed),
      underlying_autos.is_left_auto ? Length(kAuto) : Length(0, kFixed));
  return CreateClipValue(neutral_box, 1);
}

InterpolationValue CSSClipInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return nullptr;
}

InterpolationValue CSSClipInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  ClipAutos inherited_autos = GetClipAutos(*state.ParentStyle());
  conversion_checkers.push_back(InheritedAutosChecker::Create(inherited_autos));
  if (inherited_autos.is_auto)
    return nullptr;
  return CreateClipValue(state.ParentStyle()->Clip(),
                         state.ParentStyle()->EffectiveZoom());
}

static bool IsCSSAuto(const CSSValue& value) {
  return value.IsIdentifierValue() &&
         ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto;
}

static std::unique_ptr<InterpolableValue> ConvertClipComponent(
    const CSSValue& length) {
  if (IsCSSAuto(length))
    return InterpolableList::Create(0);
  return LengthInterpolationFunctions::MaybeConvertCSSValue(length)
      .interpolable_value;
}

InterpolationValue CSSClipInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsQuadValue())
    return nullptr;
  const CSSQuadValue& quad = ToCSSQuadValue(value);
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kClipComponentIndexCount);
  list->Set(kClipTop, ConvertClipComponent(*quad.Top()));
  list->Set(kClipRight, ConvertClipComponent(*quad.Right()));
  list->Set(kClipBottom, ConvertClipComponent(*quad.Bottom()));
  list->Set(kClipLeft, ConvertClipComponent(*quad.Left()));
  ClipAutos autos(IsCSSAuto(*quad.Top()), IsCSSAuto(*quad.Right()),
                  IsCSSAuto(*quad.Bottom()), IsCSSAuto(*quad.Left()));
  return InterpolationValue(std::move(list),
                            CSSClipNonInterpolableValue::Create(autos));
}

InterpolationValue
CSSClipInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  if (style.HasAutoClip())
    return nullptr;
  return CreateClipValue(style.Clip(), style.EffectiveZoom());
}

PairwiseInterpolationValue CSSClipInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const ClipAutos& start_autos =
      ToCSSClipNonInterpolableValue(*start.non_interpolable_value)
          .GetClipAutos();
  const ClipAutos& end_autos =
      ToCSSClipNonInterpolableValue(*end.non_interpolable_value).GetClipAutos();
  if (start_autos != end_autos)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

void CSSClipInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const ClipAutos& underlying_autos =
      ToCSSClipNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .GetClipAutos();
  const ClipAutos& autos =
      ToCSSClipNonInterpolableValue(*value.non_interpolable_value)
          .GetClipAutos();
  if (underlying_autos == autos)
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  else
    underlying_value_owner.Set(*this, value);
}

void CSSClipInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const ClipAutos& autos =
      ToCSSClipNonInterpolableValue(non_interpolable_value)->GetClipAutos();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  const auto& convert_index = [&list, &state](bool is_auto, size_t index) {
    if (is_auto)
      return Length(kAuto);
    return LengthInterpolationFunctions::CreateLength(
        *list.Get(index), nullptr, state.CssToLengthConversionData(),
        kValueRangeAll);
  };
  state.Style()->SetClip(
      LengthBox(convert_index(autos.is_top_auto, kClipTop),
                convert_index(autos.is_right_auto, kClipRight),
                convert_index(autos.is_bottom_auto, kClipBottom),
                convert_index(autos.is_left_auto, kClipLeft)));
}

}  // namespace blink

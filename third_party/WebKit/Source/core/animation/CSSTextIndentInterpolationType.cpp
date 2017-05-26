// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSTextIndentInterpolationType.h"

#include <memory>
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {

struct IndentMode {
  IndentMode(const TextIndentLine line, const TextIndentType type)
      : line(line), type(type) {}
  explicit IndentMode(const ComputedStyle& style)
      : line(style.GetTextIndentLine()), type(style.GetTextIndentType()) {}

  bool operator==(const IndentMode& other) const {
    return line == other.line && type == other.type;
  }
  bool operator!=(const IndentMode& other) const { return !(*this == other); }

  const TextIndentLine line;
  const TextIndentType type;
};

}  // namespace

class CSSTextIndentNonInterpolableValue : public NonInterpolableValue {
 public:
  static PassRefPtr<CSSTextIndentNonInterpolableValue> Create(
      PassRefPtr<NonInterpolableValue> length_non_interpolable_value,
      const IndentMode& mode) {
    return AdoptRef(new CSSTextIndentNonInterpolableValue(
        std::move(length_non_interpolable_value), mode));
  }

  const NonInterpolableValue* LengthNonInterpolableValue() const {
    return length_non_interpolable_value_.Get();
  }
  RefPtr<NonInterpolableValue>& LengthNonInterpolableValue() {
    return length_non_interpolable_value_;
  }
  const IndentMode& Mode() const { return mode_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSTextIndentNonInterpolableValue(
      PassRefPtr<NonInterpolableValue> length_non_interpolable_value,
      const IndentMode& mode)
      : length_non_interpolable_value_(
            std::move(length_non_interpolable_value)),
        mode_(mode) {}

  RefPtr<NonInterpolableValue> length_non_interpolable_value_;
  const IndentMode mode_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTextIndentNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSTextIndentNonInterpolableValue);

namespace {

class UnderlyingIndentModeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<UnderlyingIndentModeChecker> Create(
      const IndentMode& mode) {
    return WTF::WrapUnique(new UnderlyingIndentModeChecker(mode));
  }

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return mode_ == ToCSSTextIndentNonInterpolableValue(
                        *underlying.non_interpolable_value)
                        .Mode();
  }

 private:
  UnderlyingIndentModeChecker(const IndentMode& mode) : mode_(mode) {}

  const IndentMode mode_;
};

class InheritedIndentModeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedIndentModeChecker> Create(
      const IndentMode& mode) {
    return WTF::WrapUnique(new InheritedIndentModeChecker(mode));
  }

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return mode_ == IndentMode(*state.ParentStyle());
  }

 private:
  InheritedIndentModeChecker(const IndentMode& mode) : mode_(mode) {}

  const IndentMode mode_;
};

InterpolationValue CreateValue(const Length& length,
                               const IndentMode& mode,
                               double zoom) {
  InterpolationValue converted_length =
      LengthInterpolationFunctions::MaybeConvertLength(length, zoom);
  DCHECK(converted_length);
  return InterpolationValue(
      std::move(converted_length.interpolable_value),
      CSSTextIndentNonInterpolableValue::Create(
          std::move(converted_length.non_interpolable_value), mode));
}

}  // namespace

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  IndentMode mode =
      ToCSSTextIndentNonInterpolableValue(*underlying.non_interpolable_value)
          .Mode();
  conversion_checkers.push_back(UnderlyingIndentModeChecker::Create(mode));
  return CreateValue(Length(0, kFixed), mode, 1);
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  IndentMode mode(ComputedStyle::InitialTextIndentLine(),
                  ComputedStyle::InitialTextIndentType());
  return CreateValue(ComputedStyle::InitialTextIndent(), mode, 1);
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ComputedStyle& parent_style = *state.ParentStyle();
  IndentMode mode(parent_style);
  conversion_checkers.push_back(InheritedIndentModeChecker::Create(mode));
  return CreateValue(parent_style.TextIndent(), mode,
                     parent_style.EffectiveZoom());
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  InterpolationValue length = nullptr;
  TextIndentLine line = ComputedStyle::InitialTextIndentLine();
  TextIndentType type = ComputedStyle::InitialTextIndentType();

  for (const auto& item : ToCSSValueList(value)) {
    if (item->IsIdentifierValue() &&
        ToCSSIdentifierValue(*item).GetValueID() == CSSValueEachLine)
      line = TextIndentLine::kEachLine;
    else if (item->IsIdentifierValue() &&
             ToCSSIdentifierValue(*item).GetValueID() == CSSValueHanging)
      type = TextIndentType::kHanging;
    else
      length = LengthInterpolationFunctions::MaybeConvertCSSValue(*item);
  }
  DCHECK(length);

  return InterpolationValue(
      std::move(length.interpolable_value),
      CSSTextIndentNonInterpolableValue::Create(
          std::move(length.non_interpolable_value), IndentMode(line, type)));
}

InterpolationValue
CSSTextIndentInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateValue(style.TextIndent(), IndentMode(style),
                     style.EffectiveZoom());
}

PairwiseInterpolationValue CSSTextIndentInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  CSSTextIndentNonInterpolableValue& start_non_interpolable_value =
      ToCSSTextIndentNonInterpolableValue(*start.non_interpolable_value);
  CSSTextIndentNonInterpolableValue& end_non_interpolable_value =
      ToCSSTextIndentNonInterpolableValue(*end.non_interpolable_value);

  if (start_non_interpolable_value.Mode() != end_non_interpolable_value.Mode())
    return nullptr;

  PairwiseInterpolationValue result =
      LengthInterpolationFunctions::MergeSingles(
          InterpolationValue(
              std::move(start.interpolable_value),
              start_non_interpolable_value.LengthNonInterpolableValue()),
          InterpolationValue(
              std::move(end.interpolable_value),
              end_non_interpolable_value.LengthNonInterpolableValue()));
  result.non_interpolable_value = CSSTextIndentNonInterpolableValue::Create(
      std::move(result.non_interpolable_value),
      start_non_interpolable_value.Mode());
  return result;
}

void CSSTextIndentInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const IndentMode& underlying_mode =
      ToCSSTextIndentNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .Mode();
  const CSSTextIndentNonInterpolableValue& non_interpolable_value =
      ToCSSTextIndentNonInterpolableValue(*value.non_interpolable_value);
  const IndentMode& mode = non_interpolable_value.Mode();

  if (underlying_mode != mode) {
    underlying_value_owner.Set(*this, value);
    return;
  }

  LengthInterpolationFunctions::Composite(
      underlying_value_owner.MutableValue().interpolable_value,
      ToCSSTextIndentNonInterpolableValue(
          *underlying_value_owner.MutableValue().non_interpolable_value)
          .LengthNonInterpolableValue(),
      underlying_fraction, *value.interpolable_value,
      non_interpolable_value.LengthNonInterpolableValue());
}

void CSSTextIndentInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const CSSTextIndentNonInterpolableValue&
      css_text_indent_non_interpolable_value =
          ToCSSTextIndentNonInterpolableValue(*non_interpolable_value);
  ComputedStyle& style = *state.Style();
  style.SetTextIndent(LengthInterpolationFunctions::CreateLength(
      interpolable_value,
      css_text_indent_non_interpolable_value.LengthNonInterpolableValue(),
      state.CssToLengthConversionData(), kValueRangeAll));

  const IndentMode& mode = css_text_indent_non_interpolable_value.Mode();
  style.SetTextIndentLine(mode.line);
  style.SetTextIndentType(mode.type);
}

}  // namespace blink

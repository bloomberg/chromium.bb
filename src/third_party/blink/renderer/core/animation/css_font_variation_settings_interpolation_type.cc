// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_variation_settings_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSFontVariationSettingsNonInterpolableValue
    : public NonInterpolableValue {
 public:
  ~CSSFontVariationSettingsNonInterpolableValue() final = default;

  static scoped_refptr<CSSFontVariationSettingsNonInterpolableValue> Create(
      Vector<AtomicString> tags) {
    return base::AdoptRef(
        new CSSFontVariationSettingsNonInterpolableValue(std::move(tags)));
  }

  const Vector<AtomicString> Tags() const { return tags_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSFontVariationSettingsNonInterpolableValue(Vector<AtomicString> tags)
      : tags_(std::move(tags)) {
    DCHECK_GT(tags_.size(), 0u);
  }

  const Vector<AtomicString> tags_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(
    CSSFontVariationSettingsNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(
    CSSFontVariationSettingsNonInterpolableValue);

static const Vector<AtomicString> GetTags(
    const NonInterpolableValue& non_interpolable_value) {
  return ToCSSFontVariationSettingsNonInterpolableValue(non_interpolable_value)
      .Tags();
}

static bool TagsMatch(const NonInterpolableValue& a,
                      const NonInterpolableValue& b) {
  return GetTags(a) == GetTags(b);
}

class UnderlyingTagsChecker : public InterpolationType::ConversionChecker {
 public:
  ~UnderlyingTagsChecker() final = default;

  static std::unique_ptr<UnderlyingTagsChecker> Create(
      const Vector<AtomicString>& tags) {
    return base::WrapUnique(new UnderlyingTagsChecker(tags));
  }

 private:
  UnderlyingTagsChecker(const Vector<AtomicString>& tags) : tags_(tags) {}

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return tags_ == GetTags(*underlying.non_interpolable_value);
  }

  const Vector<AtomicString> tags_;
};

class InheritedFontVariationSettingsChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  ~InheritedFontVariationSettingsChecker() final = default;

  static std::unique_ptr<InheritedFontVariationSettingsChecker> Create(
      const FontVariationSettings* settings) {
    return base::WrapUnique(
        new InheritedFontVariationSettingsChecker(settings));
  }

 private:
  InheritedFontVariationSettingsChecker(const FontVariationSettings* settings)
      : settings_(settings) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return DataEquivalent(
        settings_.get(),
        state.ParentStyle()->GetFontDescription().VariationSettings());
  }

  scoped_refptr<const FontVariationSettings> settings_;
};

static InterpolationValue ConvertFontVariationSettings(
    const FontVariationSettings* settings) {
  if (!settings || settings->size() == 0) {
    return nullptr;
  }
  wtf_size_t length = settings->size();
  std::unique_ptr<InterpolableList> numbers = InterpolableList::Create(length);
  Vector<AtomicString> tags;
  for (wtf_size_t i = 0; i < length; ++i) {
    numbers->Set(i, InterpolableNumber::Create(settings->at(i).Value()));
    tags.push_back(settings->at(i).Tag());
  }
  return InterpolationValue(
      std::move(numbers),
      CSSFontVariationSettingsNonInterpolableValue::Create(std::move(tags)));
}

InterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(UnderlyingTagsChecker::Create(
      GetTags(*underlying.non_interpolable_value)));
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return ConvertFontVariationSettings(FontBuilder::InitialVariationSettings());
}

InterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const FontVariationSettings* inherited =
      state.ParentStyle()->GetFontDescription().VariationSettings();
  conversion_checkers.push_back(
      InheritedFontVariationSettingsChecker::Create(inherited));
  return ConvertFontVariationSettings(inherited);
}

InterpolationValue CSSFontVariationSettingsInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsValueList()) {
    return nullptr;
  }
  const CSSValueList& list = ToCSSValueList(value);
  wtf_size_t length = list.length();
  std::unique_ptr<InterpolableList> numbers = InterpolableList::Create(length);
  Vector<AtomicString> tags;
  for (wtf_size_t i = 0; i < length; ++i) {
    const cssvalue::CSSFontVariationValue& item =
        cssvalue::ToCSSFontVariationValue(list.Item(i));
    numbers->Set(i, InterpolableNumber::Create(item.Value()));
    tags.push_back(item.Tag());
  }
  return InterpolationValue(
      std::move(numbers),
      CSSFontVariationSettingsNonInterpolableValue::Create(std::move(tags)));
}

InterpolationValue CSSFontVariationSettingsInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return ConvertFontVariationSettings(
      style.GetFontDescription().VariationSettings());
}

PairwiseInterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (TagsMatch(*start.non_interpolable_value, *end.non_interpolable_value)) {
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end.interpolable_value),
                                      std::move(start.non_interpolable_value));
  }
  return nullptr;
}

void CSSFontVariationSettingsInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (TagsMatch(*underlying_value_owner.Value().non_interpolable_value,
                *value.non_interpolable_value)) {
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  } else {
    underlying_value_owner.Set(*this, value);
  }
}

void CSSFontVariationSettingsInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableList& numbers = ToInterpolableList(interpolable_value);
  const Vector<AtomicString>& tags = GetTags(*non_interpolable_value);
  DCHECK_EQ(numbers.length(), tags.size());

  scoped_refptr<FontVariationSettings> settings =
      FontVariationSettings::Create();
  wtf_size_t length = numbers.length();
  for (wtf_size_t i = 0; i < length; ++i) {
    settings->Append(FontVariationAxis(
        tags[i], ToInterpolableNumber(numbers.Get(i))->Value()));
  }
  state.GetFontBuilder().SetVariationSettings(settings);
}

}  // namespace blink

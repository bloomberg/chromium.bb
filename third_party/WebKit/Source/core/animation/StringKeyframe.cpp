// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/StringKeyframe.h"

#include "core/StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/style/ComputedStyle.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

StringKeyframe::StringKeyframe(const StringKeyframe& copy_from)
    : Keyframe(copy_from.offset_, copy_from.composite_, copy_from.easing_),
      css_property_map_(copy_from.css_property_map_->MutableCopy()),
      presentation_attribute_map_(
          copy_from.presentation_attribute_map_->MutableCopy()),
      svg_attribute_map_(copy_from.svg_attribute_map_) {}

MutableStylePropertySet::SetResult StringKeyframe::SetCSSPropertyValue(
    const AtomicString& property_name,
    const PropertyRegistry* registry,
    const String& value,
    StyleSheetContents* style_sheet_contents) {
  bool is_animation_tainted = true;
  return css_property_map_->SetProperty(property_name, registry, value, false,
                                        style_sheet_contents,
                                        is_animation_tainted);
}

MutableStylePropertySet::SetResult StringKeyframe::SetCSSPropertyValue(
    CSSPropertyID property,
    const String& value,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property, CSSPropertyInvalid);
  if (CSSAnimations::IsAnimationAffectingProperty(property)) {
    bool did_parse = true;
    bool did_change = false;
    return MutableStylePropertySet::SetResult{did_parse, did_change};
  }
  return css_property_map_->SetProperty(property, value, false,
                                        style_sheet_contents);
}

void StringKeyframe::SetCSSPropertyValue(CSSPropertyID property,
                                         const CSSValue& value) {
  DCHECK_NE(property, CSSPropertyInvalid);
  DCHECK(!CSSAnimations::IsAnimationAffectingProperty(property));
  css_property_map_->SetProperty(property, value, false);
}

void StringKeyframe::SetPresentationAttributeValue(
    CSSPropertyID property,
    const String& value,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property, CSSPropertyInvalid);
  if (!CSSAnimations::IsAnimationAffectingProperty(property))
    presentation_attribute_map_->SetProperty(property, value, false,
                                             style_sheet_contents);
}

void StringKeyframe::SetSVGAttributeValue(const QualifiedName& attribute_name,
                                          const String& value) {
  svg_attribute_map_.Set(&attribute_name, value);
}

PropertyHandleSet StringKeyframe::Properties() const {
  // This is not used in time-critical code, so we probably don't need to
  // worry about caching this result.
  PropertyHandleSet properties;
  for (unsigned i = 0; i < css_property_map_->PropertyCount(); ++i) {
    StylePropertySet::PropertyReference property_reference =
        css_property_map_->PropertyAt(i);
    DCHECK(!isShorthandProperty(property_reference.Id()))
        << "Web Animations: Encountered unexpanded shorthand CSS property ("
        << property_reference.Id() << ").";
    if (property_reference.Id() == CSSPropertyVariable)
      properties.insert(PropertyHandle(
          ToCSSCustomPropertyDeclaration(property_reference.Value())
              .GetName()));
    else
      properties.insert(PropertyHandle(property_reference.Id(), false));
  }

  for (unsigned i = 0; i < presentation_attribute_map_->PropertyCount(); ++i)
    properties.insert(
        PropertyHandle(presentation_attribute_map_->PropertyAt(i).Id(), true));

  for (const auto& key : svg_attribute_map_.Keys())
    properties.insert(PropertyHandle(*key));

  return properties;
}

PassRefPtr<Keyframe> StringKeyframe::Clone() const {
  return AdoptRef(new StringKeyframe(*this));
}

PassRefPtr<Keyframe::PropertySpecificKeyframe>
StringKeyframe::CreatePropertySpecificKeyframe(
    const PropertyHandle& property) const {
  if (property.IsCSSProperty())
    return CSSPropertySpecificKeyframe::Create(
        Offset(), &Easing(), &CssPropertyValue(property), Composite());

  if (property.IsPresentationAttribute())
    return CSSPropertySpecificKeyframe::Create(
        Offset(), &Easing(),
        &PresentationAttributeValue(property.PresentationAttribute()),
        Composite());

  DCHECK(property.IsSVGAttribute());
  return SVGPropertySpecificKeyframe::Create(
      Offset(), &Easing(), SvgPropertyValue(property.SvgAttribute()),
      Composite());
}

bool StringKeyframe::CSSPropertySpecificKeyframe::PopulateAnimatableValue(
    CSSPropertyID property,
    Element& element,
    const ComputedStyle& base_style,
    const ComputedStyle* parent_style) const {
  animatable_value_cache_ = StyleResolver::CreateAnimatableValueSnapshot(
      element, base_style, parent_style, property, value_.Get());
  return true;
}

PassRefPtr<Keyframe::PropertySpecificKeyframe>
StringKeyframe::CSSPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    PassRefPtr<TimingFunction> easing) const {
  return Create(offset, std::move(easing), nullptr, EffectModel::kCompositeAdd);
}

PassRefPtr<Keyframe::PropertySpecificKeyframe>
StringKeyframe::CSSPropertySpecificKeyframe::CloneWithOffset(
    double offset) const {
  RefPtr<CSSPropertySpecificKeyframe> clone =
      Create(offset, easing_, value_.Get(), composite_);
  clone->animatable_value_cache_ = animatable_value_cache_;
  return clone;
}

PassRefPtr<Keyframe::PropertySpecificKeyframe>
SVGPropertySpecificKeyframe::CloneWithOffset(double offset) const {
  return Create(offset, easing_, value_, composite_);
}

PassRefPtr<Keyframe::PropertySpecificKeyframe>
SVGPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    PassRefPtr<TimingFunction> easing) const {
  return Create(offset, std::move(easing), String(),
                EffectModel::kCompositeAdd);
}

}  // namespace blink

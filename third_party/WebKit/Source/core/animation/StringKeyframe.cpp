// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/StringKeyframe.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/svg/SVGElement.h"

namespace blink {

StringKeyframe::StringKeyframe(const StringKeyframe& copy_from)
    : Keyframe(copy_from.offset_, copy_from.composite_, copy_from.easing_),
      css_property_map_(copy_from.css_property_map_->MutableCopy()),
      presentation_attribute_map_(
          copy_from.presentation_attribute_map_->MutableCopy()),
      svg_attribute_map_(copy_from.svg_attribute_map_) {}

MutableCSSPropertyValueSet::SetResult StringKeyframe::SetCSSPropertyValue(
    const AtomicString& property_name,
    const PropertyRegistry* registry,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  bool is_animation_tainted = true;
  return css_property_map_->SetProperty(
      property_name, registry, value, false, secure_context_mode,
      style_sheet_contents, is_animation_tainted);
}

MutableCSSPropertyValueSet::SetResult StringKeyframe::SetCSSPropertyValue(
    CSSPropertyID property,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property, CSSPropertyInvalid);
  if (CSSAnimations::IsAnimationAffectingProperty(CSSProperty::Get(property))) {
    bool did_parse = true;
    bool did_change = false;
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }
  return css_property_map_->SetProperty(
      property, value, false, secure_context_mode, style_sheet_contents);
}

void StringKeyframe::SetCSSPropertyValue(const CSSProperty& property,
                                         const CSSValue& value) {
  DCHECK_NE(property.PropertyID(), CSSPropertyInvalid);
  DCHECK(!CSSAnimations::IsAnimationAffectingProperty(property));
  css_property_map_->SetProperty(property.PropertyID(), value, false);
}

void StringKeyframe::SetPresentationAttributeValue(
    const CSSProperty& property,
    const String& value,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet_contents) {
  DCHECK_NE(property.PropertyID(), CSSPropertyInvalid);
  if (!CSSAnimations::IsAnimationAffectingProperty(property)) {
    presentation_attribute_map_->SetProperty(property.PropertyID(), value,
                                             false, secure_context_mode,
                                             style_sheet_contents);
  }
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
    CSSPropertyValueSet::PropertyReference property_reference =
        css_property_map_->PropertyAt(i);
    const CSSProperty& property = property_reference.Property();
    DCHECK(!property.IsShorthand())
        << "Web Animations: Encountered unexpanded shorthand CSS property ("
        << property.PropertyID() << ").";
    if (property.IDEquals(CSSPropertyVariable)) {
      properties.insert(PropertyHandle(
          ToCSSCustomPropertyDeclaration(property_reference.Value())
              .GetName()));
    } else {
      properties.insert(PropertyHandle(property, false));
    }
  }

  for (unsigned i = 0; i < presentation_attribute_map_->PropertyCount(); ++i) {
    properties.insert(PropertyHandle(
        presentation_attribute_map_->PropertyAt(i).Property(), true));
  }

  for (auto* const key : svg_attribute_map_.Keys())
    properties.insert(PropertyHandle(*key));

  return properties;
}

bool StringKeyframe::HasCssProperty() const {
  PropertyHandleSet properties = Properties();
  for (const PropertyHandle& property : properties) {
    if (property.IsCSSProperty())
      return true;
  }
  return false;
}

void StringKeyframe::AddKeyframePropertiesToV8Object(
    V8ObjectBuilder& object_builder) const {
  Keyframe::AddKeyframePropertiesToV8Object(object_builder);
  for (const PropertyHandle& property : Properties()) {
    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property);
    String value;
    if (property.IsCSSProperty()) {
      value = CssPropertyValue(property).CssText();
    } else if (property.IsPresentationAttribute()) {
      const auto& attribute = property.PresentationAttribute();
      value = PresentationAttributeValue(attribute).CssText();
    } else {
      DCHECK(property.IsSVGAttribute());
      value = SvgPropertyValue(property.SvgAttribute());
    }

    object_builder.Add(property_name, value);
  }
}

scoped_refptr<Keyframe> StringKeyframe::Clone() const {
  return base::AdoptRef(new StringKeyframe(*this));
}

scoped_refptr<Keyframe::PropertySpecificKeyframe>
StringKeyframe::CreatePropertySpecificKeyframe(
    const PropertyHandle& property,
    EffectModel::CompositeOperation effect_composite,
    double offset) const {
  EffectModel::CompositeOperation composite =
      composite_.value_or(effect_composite);
  if (property.IsCSSProperty()) {
    return CSSPropertySpecificKeyframe::Create(
        offset, &Easing(), &CssPropertyValue(property), composite);
  }

  if (property.IsPresentationAttribute()) {
    return CSSPropertySpecificKeyframe::Create(
        offset, &Easing(),
        &PresentationAttributeValue(property.PresentationAttribute()),
        composite);
  }

  DCHECK(property.IsSVGAttribute());
  return SVGPropertySpecificKeyframe::Create(
      offset, &Easing(), SvgPropertyValue(property.SvgAttribute()), composite);
}

bool StringKeyframe::CSSPropertySpecificKeyframe::PopulateAnimatableValue(
    const CSSProperty& property,
    Element& element,
    const ComputedStyle& base_style,
    const ComputedStyle* parent_style) const {
  animatable_value_cache_ = StyleResolver::CreateAnimatableValueSnapshot(
      element, base_style, parent_style, property, value_.Get());
  return true;
}

scoped_refptr<Keyframe::PropertySpecificKeyframe>
StringKeyframe::CSSPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    scoped_refptr<TimingFunction> easing) const {
  return Create(offset, std::move(easing), nullptr, EffectModel::kCompositeAdd);
}

scoped_refptr<Keyframe::PropertySpecificKeyframe>
StringKeyframe::CSSPropertySpecificKeyframe::CloneWithOffset(
    double offset) const {
  scoped_refptr<CSSPropertySpecificKeyframe> clone =
      Create(offset, easing_, value_.Get(), composite_);
  clone->animatable_value_cache_ = animatable_value_cache_;
  return clone;
}

scoped_refptr<Keyframe::PropertySpecificKeyframe>
SVGPropertySpecificKeyframe::CloneWithOffset(double offset) const {
  return Create(offset, easing_, value_, composite_);
}

scoped_refptr<Keyframe::PropertySpecificKeyframe>
SVGPropertySpecificKeyframe::NeutralKeyframe(
    double offset,
    scoped_refptr<TimingFunction> easing) const {
  return Create(offset, std::move(easing), String(),
                EffectModel::kCompositeAdd);
}

}  // namespace blink

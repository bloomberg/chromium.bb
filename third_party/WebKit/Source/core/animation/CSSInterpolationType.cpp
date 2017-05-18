// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSInterpolationType.h"

#include <memory>
#include "core/StylePropertyShorthand.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/css/PropertyRegistration.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/resolver/CSSVariableResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/DataEquivalency.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class ResolvedVariableChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<ResolvedVariableChecker> Create(
      CSSPropertyID property,
      const CSSValue* variable_reference,
      const CSSValue* resolved_value) {
    return WTF::WrapUnique(new ResolvedVariableChecker(
        property, variable_reference, resolved_value));
  }

 private:
  ResolvedVariableChecker(CSSPropertyID property,
                          const CSSValue* variable_reference,
                          const CSSValue* resolved_value)
      : property_(property),
        variable_reference_(variable_reference),
        resolved_value_(resolved_value) {}

  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    // TODO(alancutter): Just check the variables referenced instead of doing a
    // full CSSValue resolve.
    bool omit_animation_tainted = false;
    const CSSValue* resolved_value =
        CSSVariableResolver::ResolveVariableReferences(
            environment.GetState(), property_, *variable_reference_,
            omit_animation_tainted);
    return DataEquivalent(resolved_value_.Get(), resolved_value);
  }

  CSSPropertyID property_;
  Persistent<const CSSValue> variable_reference_;
  Persistent<const CSSValue> resolved_value_;
};

class InheritedCustomPropertyChecker
    : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedCustomPropertyChecker> Create(
      const AtomicString& property,
      bool is_inherited_property,
      const CSSValue* inherited_value,
      const CSSValue* initial_value) {
    return WTF::WrapUnique(new InheritedCustomPropertyChecker(
        property, is_inherited_property, inherited_value, initial_value));
  }

 private:
  InheritedCustomPropertyChecker(const AtomicString& name,
                                 bool is_inherited_property,
                                 const CSSValue* inherited_value,
                                 const CSSValue* initial_value)
      : name_(name),
        is_inherited_property_(is_inherited_property),
        inherited_value_(inherited_value),
        initial_value_(initial_value) {}

  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const CSSValue* inherited_value =
        environment.GetState().ParentStyle()->GetRegisteredVariable(
            name_, is_inherited_property_);
    if (!inherited_value) {
      inherited_value = initial_value_.Get();
    }
    return DataEquivalent(inherited_value_.Get(), inherited_value);
  }

  const AtomicString& name_;
  const bool is_inherited_property_;
  Persistent<const CSSValue> inherited_value_;
  Persistent<const CSSValue> initial_value_;
};

CSSInterpolationType::CSSInterpolationType(PropertyHandle property)
    : InterpolationType(property) {
  DCHECK(!isShorthandProperty(CssProperty()));
}

void CSSInterpolationType::SetCustomPropertyRegistration(
    const PropertyRegistration& registration) {
  DCHECK(GetProperty().IsCSSCustomProperty());
  DCHECK(!registration_);
  registration_ = &registration;
}

InterpolationValue CSSInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  InterpolationValue result = MaybeConvertSingleInternal(
      keyframe, environment, underlying, conversion_checkers);
  if (result && keyframe.Composite() !=
                    EffectModel::CompositeOperation::kCompositeReplace) {
    AdditiveKeyframeHook(result);
  }
  return result;
}

InterpolationValue CSSInterpolationType::MaybeConvertSingleInternal(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const CSSValue* value = ToCSSPropertySpecificKeyframe(keyframe).Value();
  const StyleResolverState& state = environment.GetState();

  if (!value)
    return MaybeConvertNeutral(underlying, conversion_checkers);

  if (GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertCustomPropertyDeclaration(
        ToCSSCustomPropertyDeclaration(*value), state, conversion_checkers);
  }

  if (value->IsVariableReferenceValue() ||
      value->IsPendingSubstitutionValue()) {
    bool omit_animation_tainted = false;
    const CSSValue* resolved_value =
        CSSVariableResolver::ResolveVariableReferences(
            state, CssProperty(), *value, omit_animation_tainted);
    conversion_checkers.push_back(
        ResolvedVariableChecker::Create(CssProperty(), value, resolved_value));
    value = resolved_value;
  }

  if (value->IsInitialValue() ||
      (value->IsUnsetValue() &&
       !CSSPropertyMetadata::IsInheritedProperty(CssProperty()))) {
    return MaybeConvertInitial(state, conversion_checkers);
  }

  if (value->IsInheritedValue() ||
      (value->IsUnsetValue() &&
       CSSPropertyMetadata::IsInheritedProperty(CssProperty()))) {
    return MaybeConvertInherit(state, conversion_checkers);
  }

  return MaybeConvertValue(*value, &state, conversion_checkers);
}

InterpolationValue CSSInterpolationType::MaybeConvertCustomPropertyDeclaration(
    const CSSCustomPropertyDeclaration& declaration,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  InterpolationValue result = MaybeConvertCustomPropertyDeclarationInternal(
      declaration, state, conversion_checkers);
  if (result) {
    return result;
  }

  // TODO(alancutter): Make explicit and assert in code that this falls back to
  // the default CSSValueInterpolationType handler.
  // This might involve making the "catch-all" InterpolationType explicit
  // e.g. add bool InterpolationType::isCatchAll().
  return MaybeConvertValue(declaration, &state, conversion_checkers);
}

InterpolationValue
CSSInterpolationType::MaybeConvertCustomPropertyDeclarationInternal(
    const CSSCustomPropertyDeclaration& declaration,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const AtomicString& name = declaration.GetName();
  DCHECK_EQ(GetProperty().CustomPropertyName(), name);

  if (!declaration.Value()) {
    // Unregistered custom properties inherit:
    // https://www.w3.org/TR/css-variables-1/#defining-variables
    bool is_inherited_property =
        registration_ ? registration_->Inherits() : true;
    DCHECK(declaration.IsInitial(is_inherited_property) ||
           declaration.IsInherit(is_inherited_property));

    if (!registration_) {
      return nullptr;
    }

    const CSSValue* value = nullptr;
    if (declaration.IsInitial(is_inherited_property)) {
      value = registration_->Initial();
    } else {
      value = state.ParentStyle()->GetRegisteredVariable(name,
                                                         is_inherited_property);
      if (!value) {
        value = registration_->Initial();
      }
      conversion_checkers.push_back(InheritedCustomPropertyChecker::Create(
          name, is_inherited_property, value, registration_->Initial()));
    }
    if (!value) {
      return nullptr;
    }

    return MaybeConvertValue(*value, &state, conversion_checkers);
  }

  if (declaration.Value()->NeedsVariableResolution()) {
    // TODO(alancutter): Support smooth interpolation with var() values for
    // registered custom properties. This requires integrating animated custom
    // property value application with the CSSVariableResolver to apply them in
    // the appropriate order defined by the chain of var() dependencies.
    // All CSSInterpolationTypes should fail convertion here except for
    // CSSValueInterpolationType.
    return nullptr;
  }

  if (registration_) {
    const CSSValue* parsed_value =
        declaration.Value()->ParseForSyntax(registration_->Syntax());
    if (parsed_value) {
      return MaybeConvertValue(*parsed_value, &state, conversion_checkers);
    }
  }

  return nullptr;
}

InterpolationValue CSSInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const ComputedStyle& style = environment.Style();
  if (!GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertStandardPropertyUnderlyingValue(style);
  }

  const PropertyHandle property = GetProperty();
  const AtomicString& name = property.CustomPropertyName();
  if (!registration_) {
    return nullptr;
  }
  const CSSValue* underlying_value =
      style.GetRegisteredVariable(name, registration_->Inherits());
  if (!underlying_value) {
    underlying_value = registration_->Initial();
  }
  if (!underlying_value) {
    return nullptr;
  }
  // TODO(alancutter): Remove the need for passing in conversion checkers.
  ConversionCheckers dummy_conversion_checkers;
  return MaybeConvertValue(*underlying_value, nullptr,
                           dummy_conversion_checkers);
}

void CSSInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  StyleResolverState& state = environment.GetState();

  if (GetProperty().IsCSSCustomProperty()) {
    ApplyCustomPropertyValue(interpolable_value, non_interpolable_value, state);
    return;
  }
  ApplyStandardPropertyValue(interpolable_value, non_interpolable_value, state);
}

void CSSInterpolationType::ApplyCustomPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  DCHECK(GetProperty().IsCSSCustomProperty());

  const CSSValue* css_value =
      CreateCSSValue(interpolable_value, non_interpolable_value, state);
  if (css_value->IsCustomPropertyDeclaration()) {
    StyleBuilder::ApplyProperty(CssProperty(), state, *css_value);
    return;
  }

  // TODO(alancutter): Defer tokenization of the CSSValue until it is needed.
  String string_value = css_value->CssText();
  CSSTokenizer tokenizer(string_value);
  bool is_animation_tainted = true;
  bool needs_variable_resolution = false;
  RefPtr<CSSVariableData> variable_data = CSSVariableData::Create(
      tokenizer.TokenRange(), is_animation_tainted, needs_variable_resolution);
  ComputedStyle& style = *state.Style();
  const PropertyHandle property = GetProperty();
  const AtomicString& property_name = property.CustomPropertyName();
  DCHECK(registration_);
  if (registration_->Inherits()) {
    style.SetResolvedInheritedVariable(property_name, std::move(variable_data),
                                       css_value);
  } else {
    style.SetResolvedNonInheritedVariable(property_name,
                                          std::move(variable_data), css_value);
  }
}

}  // namespace blink

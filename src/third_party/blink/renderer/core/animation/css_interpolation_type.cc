// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

class ResolvedVariableChecker : public CSSInterpolationType::ConversionChecker {
 public:
  ResolvedVariableChecker(CSSPropertyID property,
                          const CSSValue* variable_reference,
                          const CSSValue* resolved_value)
      : property_(property),
        variable_reference_(variable_reference),
        resolved_value_(resolved_value) {}

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    // TODO(alancutter): Just check the variables referenced instead of doing a
    // full CSSValue resolve.
    bool omit_animation_tainted = false;
    const CSSValue* resolved_value = nullptr;
    if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
      resolved_value = css_environment.Resolve(
          PropertyHandle(CSSProperty::Get(property_)), variable_reference_);
    } else {
      const StyleResolverState& state = css_environment.GetState();
      resolved_value = CSSVariableResolver(state).ResolveVariableReferences(
          property_, *variable_reference_, omit_animation_tainted);
    }
    return DataEquivalent(resolved_value_.Get(), resolved_value);
  }

  CSSPropertyID property_;
  Persistent<const CSSValue> variable_reference_;
  Persistent<const CSSValue> resolved_value_;
};

class InheritedCustomPropertyChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedCustomPropertyChecker(const AtomicString& name,
                                 bool is_inherited_property,
                                 const CSSValue* inherited_value,
                                 const CSSValue* initial_value)
      : name_(name),
        is_inherited_property_(is_inherited_property),
        inherited_value_(inherited_value),
        initial_value_(initial_value) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const CSSValue* inherited_value =
        state.ParentStyle()->GetVariableValue(name_, is_inherited_property_);
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

class ResolvedRegisteredCustomPropertyChecker
    : public InterpolationType::ConversionChecker {
 public:
  ResolvedRegisteredCustomPropertyChecker(
      const CSSCustomPropertyDeclaration& declaration,
      scoped_refptr<CSSVariableData> resolved_tokens)
      : declaration_(declaration),
        resolved_tokens_(std::move(resolved_tokens)) {}

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    scoped_refptr<CSSVariableData> resolved_tokens = nullptr;
    if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
      const CSSValue* resolved = css_environment.Resolve(
          PropertyHandle(declaration_->GetName()), declaration_);
      if (const auto* decl = DynamicTo<CSSCustomPropertyDeclaration>(resolved))
        resolved_tokens = decl->Value();
    } else {
      DCHECK(css_environment.HasVariableResolver());
      bool cycle_detected = false;
      resolved_tokens = To<CSSInterpolationEnvironment>(environment)
                            .VariableResolver()
                            .ResolveCustomPropertyAnimationKeyframe(
                                *declaration_, cycle_detected);
      DCHECK(!cycle_detected);
    }
    return DataEquivalent(resolved_tokens, resolved_tokens_);
  }

  Persistent<const CSSCustomPropertyDeclaration> declaration_;
  scoped_refptr<CSSVariableData> resolved_tokens_;
};

class RevertChecker : public CSSInterpolationType::ConversionChecker {
 public:
  RevertChecker(const PropertyHandle& property_handle,
                const CSSValue* resolved_value)
      : property_handle_(property_handle), resolved_value_(resolved_value) {
    DCHECK(RuntimeEnabledFeatures::CSSRevertEnabled());
  }

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    const CSSValue* current_resolved_value = css_environment.Resolve(
        property_handle_, cssvalue::CSSRevertValue::Create());
    return DataEquivalent(resolved_value_.Get(), current_resolved_value);
  }

  PropertyHandle property_handle_;
  Persistent<const CSSValue> resolved_value_;
};

CSSInterpolationType::CSSInterpolationType(
    PropertyHandle property,
    const PropertyRegistration* registration)
    : InterpolationType(property), registration_(registration) {
  DCHECK(!GetProperty().IsCSSCustomProperty() || registration);
  DCHECK(!CssProperty().IsShorthand());
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
    return PreInterpolationCompositeIfNeeded(std::move(result), underlying,
                                             keyframe.Composite(),
                                             conversion_checkers);
  }
  return result;
}

InterpolationValue CSSInterpolationType::MaybeConvertSingleInternal(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const CSSValue* value = To<CSSPropertySpecificKeyframe>(keyframe).Value();
  const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
  const StyleResolverState& state = css_environment.GetState();

  if (!value)
    return MaybeConvertNeutral(underlying, conversion_checkers);

  if (GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertCustomPropertyDeclaration(
        To<CSSCustomPropertyDeclaration>(*value), environment,
        conversion_checkers);
  }

  if (value->IsVariableReferenceValue() ||
      value->IsPendingSubstitutionValue()) {
    const CSSValue* resolved_value = nullptr;
    if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
      resolved_value = css_environment.Resolve(GetProperty(), value);
    } else {
      bool omit_animation_tainted = false;
      resolved_value = CSSVariableResolver(state).ResolveVariableReferences(
          CssProperty().PropertyID(), *value, omit_animation_tainted);
    }

    DCHECK(resolved_value);
    conversion_checkers.push_back(std::make_unique<ResolvedVariableChecker>(
        CssProperty().PropertyID(), value, resolved_value));
    value = resolved_value;
  }

  if (value->IsRevertValue()) {
    DCHECK(RuntimeEnabledFeatures::CSSRevertEnabled());
    value = css_environment.Resolve(GetProperty(), value);
    DCHECK(value);
    conversion_checkers.push_back(
        std::make_unique<RevertChecker>(GetProperty(), value));
  }

  bool is_inherited = CssProperty().IsInherited();
  if (value->IsInitialValue() || (value->IsUnsetValue() && !is_inherited)) {
    return MaybeConvertInitial(state, conversion_checkers);
  }

  if (value->IsInheritedValue() || (value->IsUnsetValue() && is_inherited)) {
    return MaybeConvertInherit(state, conversion_checkers);
  }

  return MaybeConvertValue(*value, &state, conversion_checkers);
}

InterpolationValue CSSInterpolationType::MaybeConvertCustomPropertyDeclaration(
    const CSSCustomPropertyDeclaration& declaration,
    const InterpolationEnvironment& environment,
    ConversionCheckers& conversion_checkers) const {
  const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
  const StyleResolverState& state = css_environment.GetState();

  const AtomicString& name = declaration.GetName();
  DCHECK_EQ(GetProperty().CustomPropertyName(), name);

  if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
    const CSSValue* value = &declaration;
    value = css_environment.Resolve(GetProperty(), value);
    DCHECK(value) << "CSSVarCycleInterpolationType should have handled nullptr";

    if (declaration.IsRevert()) {
      DCHECK(RuntimeEnabledFeatures::CSSRevertEnabled());
      conversion_checkers.push_back(
          std::make_unique<RevertChecker>(GetProperty(), value));
    }
    if (const auto* resolved_declaration =
            DynamicTo<CSSCustomPropertyDeclaration>(value)) {
      // If Resolve returned a different CSSCustomPropertyDeclaration, var()
      // references were substituted.
      if (resolved_declaration != &declaration) {
        conversion_checkers.push_back(
            std::make_unique<ResolvedRegisteredCustomPropertyChecker>(
                declaration, resolved_declaration->Value()));
      }
    }

    // Unfortunately we transport CSS-wide keywords inside the
    // CSSCustomPropertyDeclaration. Expand those keywords into real CSSValues
    // if present.
    bool is_inherited = Registration().Inherits();
    if (const auto* resolved_declaration =
            DynamicTo<CSSCustomPropertyDeclaration>(value)) {
      if (resolved_declaration->IsInitial(is_inherited))
        value = CSSInitialValue::Create();
      else if (resolved_declaration->IsInherit(is_inherited))
        value = CSSInheritedValue::Create();
    }

    // Handle CSS-wide keywords (except 'revert', which should have been
    // handled already).
    DCHECK(!value->IsRevertValue());
    if (value->IsInitialValue() || (value->IsUnsetValue() && !is_inherited)) {
      value = Registration().Initial();
    } else if (value->IsInheritedValue() ||
               (value->IsUnsetValue() && is_inherited)) {
      value = state.ParentStyle()->GetVariableValue(name, is_inherited);
      if (!value) {
        value = Registration().Initial();
      }
      conversion_checkers.push_back(
          std::make_unique<InheritedCustomPropertyChecker>(
              name, is_inherited, value, Registration().Initial()));
    }

    if (const auto* resolved_declaration =
            DynamicTo<CSSCustomPropertyDeclaration>(value)) {
      DCHECK(resolved_declaration->Value());
      value = resolved_declaration->Value()->ParseForSyntax(
          registration_->Syntax(), state.GetDocument().GetSecureContextMode());
      if (!value)
        return nullptr;
    }

    DCHECK(value);
    return MaybeConvertValue(*value, &state, conversion_checkers);
  }

  if (!declaration.Value()) {
    bool is_inherited_property = Registration().Inherits();
    DCHECK(declaration.IsInitial(is_inherited_property) ||
           declaration.IsInherit(is_inherited_property));

    const CSSValue* value = nullptr;
    if (declaration.IsInitial(is_inherited_property)) {
      value = Registration().Initial();
    } else {
      value =
          state.ParentStyle()->GetVariableValue(name, is_inherited_property);
      if (!value) {
        value = Registration().Initial();
      }
      conversion_checkers.push_back(
          std::make_unique<InheritedCustomPropertyChecker>(
              name, is_inherited_property, value, Registration().Initial()));
    }
    if (!value) {
      return nullptr;
    }

    return MaybeConvertValue(*value, &state, conversion_checkers);
  }

  scoped_refptr<CSSVariableData> resolved_tokens;
  if (declaration.Value()->NeedsVariableResolution()) {
    CSSVariableResolver& variable_resolver = css_environment.VariableResolver();
    bool cycle_detected = false;
    resolved_tokens = variable_resolver.ResolveCustomPropertyAnimationKeyframe(
        declaration, cycle_detected);
    DCHECK(!cycle_detected);
    conversion_checkers.push_back(
        std::make_unique<ResolvedRegisteredCustomPropertyChecker>(
            declaration, resolved_tokens));
  } else {
    resolved_tokens = declaration.Value();
  }
  const CSSValue* resolved_value =
      resolved_tokens ? resolved_tokens->ParseForSyntax(
                            registration_->Syntax(),
                            state.GetDocument().GetSecureContextMode())
                      : nullptr;
  if (!resolved_value) {
    return nullptr;
  }
  return MaybeConvertValue(*resolved_value, &state, conversion_checkers);
}

InterpolationValue CSSInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const ComputedStyle& style =
      To<CSSInterpolationEnvironment>(environment).Style();
  if (!GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertStandardPropertyUnderlyingValue(style);
  }

  const PropertyHandle property = GetProperty();
  const AtomicString& name = property.CustomPropertyName();
  const CSSValue* underlying_value =
      style.GetVariableValue(name, Registration().Inherits());
  if (!underlying_value)
    return nullptr;
  // TODO(alancutter): Remove the need for passing in conversion checkers.
  ConversionCheckers dummy_conversion_checkers;
  return MaybeConvertValue(*underlying_value, nullptr,
                           dummy_conversion_checkers);
}

void CSSInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  StyleResolverState& state =
      To<CSSInterpolationEnvironment>(environment).GetState();

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
  DCHECK(!css_value->IsCustomPropertyDeclaration());

  // TODO(alancutter): Defer tokenization of the CSSValue until it is needed.
  String string_value = css_value->CssText();
  CSSTokenizer tokenizer(string_value);
  const auto tokens = tokenizer.TokenizeToEOF();
  bool is_animation_tainted = true;
  bool needs_variable_resolution = false;
  scoped_refptr<CSSVariableData> variable_data = CSSVariableData::Create(
      CSSParserTokenRange(tokens), is_animation_tainted,
      needs_variable_resolution, KURL(), WTF::TextEncoding());
  const PropertyHandle property = GetProperty();

  if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
    // When cascade is enabled, we need to go through the StyleBuilder,
    // since the computed value is produced during ApplyProperty. (Without
    // this, we'd end up with values like 10em on the computed style).
    //
    // TODO(andruud): Avoid making the CSSCustomPropertyDeclaration by allowing
    // any CSSValue in CustomProperty::ApplyValue.
    const CSSValue* value = MakeGarbageCollected<CSSCustomPropertyDeclaration>(
        property.CustomPropertyName(), std::move(variable_data));
    StyleBuilder::ApplyProperty(GetProperty().GetCSSPropertyName(), state,
                                *value);
    return;
  }

  ComputedStyle& style = *state.Style();
  const AtomicString& property_name = property.CustomPropertyName();
  bool inherits = Registration().Inherits();
  style.SetVariableData(property_name, std::move(variable_data), inherits);
  style.SetVariableValue(property_name, css_value, inherits);
}

}  // namespace blink

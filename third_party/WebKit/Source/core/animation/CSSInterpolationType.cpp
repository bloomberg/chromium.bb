// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSInterpolationType.h"

#include "core/StylePropertyShorthand.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/resolver/CSSVariableResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/DataEquivalency.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class ResolvedVariableChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<ResolvedVariableChecker> create(
      CSSPropertyID property,
      const CSSValue* variableReference,
      const CSSValue* resolvedValue) {
    return WTF::wrapUnique(new ResolvedVariableChecker(
        property, variableReference, resolvedValue));
  }

 private:
  ResolvedVariableChecker(CSSPropertyID property,
                          const CSSValue* variableReference,
                          const CSSValue* resolvedValue)
      : m_property(property),
        m_variableReference(variableReference),
        m_resolvedValue(resolvedValue) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    // TODO(alancutter): Just check the variables referenced instead of doing a
    // full CSSValue resolve.
    bool omitAnimationTainted = false;
    const CSSValue* resolvedValue =
        CSSVariableResolver::resolveVariableReferences(
            environment.state(), m_property, *m_variableReference,
            omitAnimationTainted);
    return m_resolvedValue->equals(*resolvedValue);
  }

  CSSPropertyID m_property;
  Persistent<const CSSValue> m_variableReference;
  Persistent<const CSSValue> m_resolvedValue;
};

class InheritedCustomPropertyChecker
    : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedCustomPropertyChecker> create(
      const AtomicString& property,
      bool isInheritedProperty,
      const CSSValue* inheritedValue,
      const CSSValue* initialValue) {
    return WTF::wrapUnique(new InheritedCustomPropertyChecker(
        property, isInheritedProperty, inheritedValue, initialValue));
  }

 private:
  InheritedCustomPropertyChecker(const AtomicString& name,
                                 bool isInheritedProperty,
                                 const CSSValue* inheritedValue,
                                 const CSSValue* initialValue)
      : m_name(name),
        m_isInheritedProperty(isInheritedProperty),
        m_inheritedValue(inheritedValue),
        m_initialValue(initialValue) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const CSSValue* inheritedValue =
        environment.state().parentStyle()->getRegisteredVariable(
            m_name, m_isInheritedProperty);
    if (!inheritedValue) {
      inheritedValue = m_initialValue.get();
    }
    if (inheritedValue == m_inheritedValue.get()) {
      return true;
    }
    if (!inheritedValue || !m_inheritedValue) {
      return false;
    }
    return m_inheritedValue->equals(*inheritedValue);
  }

  const AtomicString& m_name;
  const bool m_isInheritedProperty;
  Persistent<const CSSValue> m_inheritedValue;
  Persistent<const CSSValue> m_initialValue;
};

CSSInterpolationType::CSSInterpolationType(PropertyHandle property)
    : InterpolationType(property) {
  DCHECK(!isShorthandProperty(cssProperty()));
}

InterpolationValue CSSInterpolationType::maybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  InterpolationValue result = maybeConvertSingleInternal(
      keyframe, environment, underlying, conversionCheckers);
  if (result &&
      keyframe.composite() !=
          EffectModel::CompositeOperation::CompositeReplace) {
    additiveKeyframeHook(result);
  }
  return result;
}

InterpolationValue CSSInterpolationType::maybeConvertSingleInternal(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  const CSSValue* value = toCSSPropertySpecificKeyframe(keyframe).value();
  const StyleResolverState& state = environment.state();

  if (!value)
    return maybeConvertNeutral(underlying, conversionCheckers);

  if (getProperty().isCSSCustomProperty()) {
    return maybeConvertCustomPropertyDeclaration(
        toCSSCustomPropertyDeclaration(*value), state, conversionCheckers);
  }

  if (value->isVariableReferenceValue() ||
      value->isPendingSubstitutionValue()) {
    bool omitAnimationTainted = false;
    const CSSValue* resolvedValue =
        CSSVariableResolver::resolveVariableReferences(
            state, cssProperty(), *value, omitAnimationTainted);
    conversionCheckers.push_back(
        ResolvedVariableChecker::create(cssProperty(), value, resolvedValue));
    value = resolvedValue;
  }

  if (value->isInitialValue() ||
      (value->isUnsetValue() &&
       !CSSPropertyMetadata::isInheritedProperty(cssProperty()))) {
    return maybeConvertInitial(state, conversionCheckers);
  }

  if (value->isInheritedValue() ||
      (value->isUnsetValue() &&
       CSSPropertyMetadata::isInheritedProperty(cssProperty()))) {
    return maybeConvertInherit(state, conversionCheckers);
  }

  return maybeConvertValue(*value, state, conversionCheckers);
}

const PropertyRegistry::Registration* getRegistration(
    const StyleResolverState& state,
    const AtomicString& propertyName) {
  const PropertyRegistry* registry = state.document().propertyRegistry();
  if (registry) {
    return registry->registration(propertyName);
  }
  return nullptr;
}

InterpolationValue CSSInterpolationType::maybeConvertCustomPropertyDeclaration(
    const CSSCustomPropertyDeclaration& declaration,
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  InterpolationValue result = maybeConvertCustomPropertyDeclarationInternal(
      declaration, state, conversionCheckers);
  if (result) {
    return result;
  }

  // TODO(alancutter): Make explicit and assert in code that this falls back to
  // the default CSSValueInterpolationType handler.
  // This might involve making the "catch-all" InterpolationType explicit
  // e.g. add bool InterpolationType::isCatchAll().
  return maybeConvertValue(declaration, state, conversionCheckers);
}

InterpolationValue
CSSInterpolationType::maybeConvertCustomPropertyDeclarationInternal(
    const CSSCustomPropertyDeclaration& declaration,
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  const AtomicString& name = declaration.name();
  DCHECK_EQ(getProperty().customPropertyName(), name);

  const PropertyRegistry::Registration* registration =
      getRegistration(state, name);

  if (!declaration.value()) {
    // Unregistered custom properties inherit:
    // https://www.w3.org/TR/css-variables-1/#defining-variables
    bool isInheritedProperty = registration ? registration->inherits() : true;
    DCHECK(declaration.isInitial(isInheritedProperty) ||
           declaration.isInherit(isInheritedProperty));

    if (!registration) {
      return nullptr;
    }

    const CSSValue* value = nullptr;
    if (declaration.isInitial(isInheritedProperty)) {
      value = registration->initial();
    } else {
      const CSSValue* value =
          state.parentStyle()->getRegisteredVariable(name, isInheritedProperty);
      if (!value) {
        value = registration->initial();
      }
      conversionCheckers.push_back(InheritedCustomPropertyChecker::create(
          name, isInheritedProperty, value, registration->initial()));
    }
    if (!value) {
      return nullptr;
    }

    return maybeConvertValue(*value, state, conversionCheckers);
  }

  if (declaration.value()->needsVariableResolution()) {
    // TODO(alancutter): Support smooth interpolation with var() values for
    // registered custom properties. This requires integrating animated custom
    // property value application with the CSSVariableResolver to apply them in
    // the appropriate order defined by the chain of var() dependencies.
    // All CSSInterpolationTypes should fail convertion here except for
    // CSSValueInterpolationType.
    return nullptr;
  }

  if (registration) {
    const CSSValue* parsedValue =
        declaration.value()->parseForSyntax(registration->syntax());
    if (parsedValue) {
      return maybeConvertValue(*parsedValue, state, conversionCheckers);
    }
  }

  return nullptr;
}

InterpolationValue CSSInterpolationType::maybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const StyleResolverState& state = environment.state();
  if (!getProperty().isCSSCustomProperty()) {
    return maybeConvertStandardPropertyUnderlyingValue(state);
  }

  const PropertyHandle property = getProperty();
  const AtomicString& name = property.customPropertyName();
  const PropertyRegistry::Registration* registration =
      getRegistration(state, name);
  if (!registration) {
    return nullptr;
  }
  const CSSValue* underlyingValue =
      state.style()->getRegisteredVariable(name, registration->inherits());
  if (!underlyingValue) {
    underlyingValue = registration->initial();
  }
  if (!underlyingValue) {
    return nullptr;
  }
  // TODO(alancutter): Remove the need for passing in conversion checkers.
  ConversionCheckers dummyConversionCheckers;
  return maybeConvertValue(*underlyingValue, environment.state(),
                           dummyConversionCheckers);
}

void CSSInterpolationType::apply(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    InterpolationEnvironment& environment) const {
  StyleResolverState& state = environment.state();

  if (getProperty().isCSSCustomProperty()) {
    applyCustomPropertyValue(interpolableValue, nonInterpolableValue, state);
    return;
  }
  applyStandardPropertyValue(interpolableValue, nonInterpolableValue, state);
}

void CSSInterpolationType::applyCustomPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  DCHECK(getProperty().isCSSCustomProperty());

  const CSSValue* cssValue =
      createCSSValue(interpolableValue, nonInterpolableValue, state);
  if (cssValue->isCustomPropertyDeclaration()) {
    StyleBuilder::applyProperty(cssProperty(), state, *cssValue);
    return;
  }

  // TODO(alancutter): Defer tokenization of the CSSValue until it is needed.
  String stringValue = cssValue->cssText();
  CSSTokenizer tokenizer(stringValue);
  bool isAnimationTainted = true;
  bool needsVariableResolution = false;
  RefPtr<CSSVariableData> variableData = CSSVariableData::create(
      tokenizer.tokenRange(), isAnimationTainted, needsVariableResolution);
  ComputedStyle& style = *state.style();
  const PropertyHandle property = getProperty();
  const AtomicString& propertyName = property.customPropertyName();
  const PropertyRegistry::Registration* registration =
      getRegistration(state, propertyName);
  DCHECK(registration);
  if (registration->inherits()) {
    style.setResolvedInheritedVariable(propertyName, variableData.release(),
                                       cssValue);
  } else {
    style.setResolvedNonInheritedVariable(propertyName, variableData.release(),
                                          cssValue);
  }
}

}  // namespace blink

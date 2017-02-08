// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistration.h"

#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/PropertyDescriptor.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleChangeReason.h"

namespace blink {

static InterpolationTypes setRegistrationOnCSSInterpolationTypes(
    CSSInterpolationTypes cssInterpolationTypes,
    const PropertyRegistration& registration) {
  InterpolationTypes result;
  for (auto& cssInterpolationType : cssInterpolationTypes) {
    cssInterpolationType->setCustomPropertyRegistration(registration);
    result.push_back(std::move(cssInterpolationType));
  }
  return result;
}

PropertyRegistration::PropertyRegistration(
    const CSSSyntaxDescriptor& syntax,
    bool inherits,
    const CSSValue* initial,
    PassRefPtr<CSSVariableData> initialVariableData,
    CSSInterpolationTypes cssInterpolationTypes)
    : m_syntax(syntax),
      m_inherits(inherits),
      m_initial(initial),
      m_initialVariableData(initialVariableData),
      m_interpolationTypes(setRegistrationOnCSSInterpolationTypes(
          std::move(cssInterpolationTypes),
          *this)) {}

static bool computationallyIndependent(const CSSValue& value) {
  DCHECK(!value.isCSSWideKeyword());

  if (value.isVariableReferenceValue())
    return !toCSSVariableReferenceValue(value)
                .variableDataValue()
                ->needsVariableResolution();

  if (value.isValueList()) {
    for (const CSSValue* innerValue : toCSSValueList(value)) {
      if (!computationallyIndependent(*innerValue))
        return false;
    }
    return true;
  }

  if (value.isPrimitiveValue()) {
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (!primitiveValue.isLength() &&
        !primitiveValue.isCalculatedPercentageWithLength())
      return true;

    CSSPrimitiveValue::CSSLengthArray lengthArray;
    primitiveValue.accumulateLengthArray(lengthArray);
    for (size_t i = 0; i < lengthArray.values.size(); i++) {
      if (lengthArray.typeFlags.get(i) &&
          i != CSSPrimitiveValue::UnitTypePixels &&
          i != CSSPrimitiveValue::UnitTypePercentage)
        return false;
    }
    return true;
  }

  // TODO(timloh): Images and transform-function values can also contain
  // lengths.

  return true;
}

void PropertyRegistration::registerProperty(
    ScriptState* scriptState,
    const PropertyDescriptor& descriptor,
    ExceptionState& exceptionState) {
  // Bindings code ensures these are set.
  DCHECK(descriptor.hasName());
  DCHECK(descriptor.hasInherits());
  DCHECK(descriptor.hasSyntax());

  String name = descriptor.name();
  if (!CSSVariableParser::isValidVariableName(name)) {
    exceptionState.throwDOMException(
        SyntaxError, "Custom property names must start with '--'.");
    return;
  }
  AtomicString atomicName(name);
  Document* document = toDocument(scriptState->getExecutionContext());
  PropertyRegistry& registry = *document->propertyRegistry();
  if (registry.registration(atomicName)) {
    exceptionState.throwDOMException(
        InvalidModificationError,
        "The name provided has already been registered.");
    return;
  }

  CSSSyntaxDescriptor syntaxDescriptor(descriptor.syntax());
  if (!syntaxDescriptor.isValid()) {
    exceptionState.throwDOMException(
        SyntaxError,
        "The syntax provided is not a valid custom property syntax.");
    return;
  }

  CSSInterpolationTypes cssInterpolationTypes =
      CSSInterpolationTypesMap::createCSSInterpolationTypesForSyntax(
          atomicName, syntaxDescriptor);

  if (descriptor.hasInitialValue()) {
    CSSTokenizer tokenizer(descriptor.initialValue());
    bool isAnimationTainted = false;
    const CSSValue* initial = syntaxDescriptor.parse(
        tokenizer.tokenRange(),
        document->elementSheet().contents()->parserContext(),
        isAnimationTainted);
    if (!initial) {
      exceptionState.throwDOMException(
          SyntaxError,
          "The initial value provided does not parse for the given syntax.");
      return;
    }
    if (!computationallyIndependent(*initial)) {
      exceptionState.throwDOMException(
          SyntaxError,
          "The initial value provided is not computationally independent.");
      return;
    }
    initial =
        &StyleBuilderConverter::convertRegisteredPropertyInitialValue(*initial);
    RefPtr<CSSVariableData> initialVariableData = CSSVariableData::create(
        tokenizer.tokenRange(), isAnimationTainted, false);
    registry.registerProperty(
        atomicName, syntaxDescriptor, descriptor.inherits(), initial,
        std::move(initialVariableData), std::move(cssInterpolationTypes));
  } else {
    if (!syntaxDescriptor.isTokenStream()) {
      exceptionState.throwDOMException(
          SyntaxError,
          "An initial value must be provided if the syntax is not '*'");
      return;
    }
    registry.registerProperty(atomicName, syntaxDescriptor,
                              descriptor.inherits(), nullptr, nullptr,
                              std::move(cssInterpolationTypes));
  }

  // TODO(timloh): Invalidate only elements with this custom property set
  document->setNeedsStyleRecalc(SubtreeStyleChange,
                                StyleChangeReasonForTracing::create(
                                    StyleChangeReason::PropertyRegistration));
}

}  // namespace blink

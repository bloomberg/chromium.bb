// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/CSSVariableResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/CSSVariableData.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "wtf/Vector.h"

namespace blink {

bool CSSVariableResolver::resolveFallback(CSSParserTokenRange range,
                                          Vector<CSSParserToken>& result) {
  if (range.atEnd())
    return false;
  ASSERT(range.peek().type() == CommaToken);
  range.consume();
  return resolveTokenRange(range, result);
}

CSSVariableData* CSSVariableResolver::valueForCustomProperty(
    AtomicString name) {
  if (m_variablesSeen.contains(name)) {
    m_cycleStartPoints.add(name);
    return nullptr;
  }

  DCHECK(m_registry || !RuntimeEnabledFeatures::cssVariables2Enabled());
  const PropertyRegistry::Registration* registration =
      m_registry ? m_registry->registration(name) : nullptr;

  CSSVariableData* variableData = nullptr;
  if (!registration || registration->inherits()) {
    if (m_inheritedVariables)
      variableData = m_inheritedVariables->getVariable(name);
  } else {
    variableData = m_nonInheritedVariables->getVariable(name);
  }
  if (!variableData)
    return registration ? registration->initialVariableData() : nullptr;
  if (!variableData->needsVariableResolution())
    return variableData;

  RefPtr<CSSVariableData> newVariableData =
      resolveCustomProperty(name, *variableData);
  if (!registration) {
    m_inheritedVariables->setVariable(name, newVariableData);
    return newVariableData.get();
  }

  const CSSValue* parsedValue = nullptr;
  if (newVariableData) {
    parsedValue = newVariableData->parseForSyntax(registration->syntax());
    if (parsedValue)
      parsedValue = &StyleBuilderConverter::convertRegisteredPropertyValue(
          m_styleResolverState, *parsedValue);
    else
      newVariableData = nullptr;
  }
  if (registration->inherits()) {
    m_inheritedVariables->setVariable(name, newVariableData);
    m_inheritedVariables->setRegisteredVariable(name, parsedValue);
  } else {
    m_nonInheritedVariables->setVariable(name, newVariableData);
    m_nonInheritedVariables->setRegisteredVariable(name, parsedValue);
  }
  if (!newVariableData)
    return registration->initialVariableData();
  return newVariableData.get();
}

PassRefPtr<CSSVariableData> CSSVariableResolver::resolveCustomProperty(
    AtomicString name,
    const CSSVariableData& variableData) {
  ASSERT(variableData.needsVariableResolution());

  Vector<CSSParserToken> tokens;
  m_variablesSeen.add(name);
  bool success = resolveTokenRange(variableData.tokens(), tokens);
  m_variablesSeen.remove(name);

  // The old variable data holds onto the backing string the new resolved CSSVariableData
  // relies on. Ensure it will live beyond us overwriting the RefPtr in StyleInheritedVariables.
  ASSERT(variableData.refCount() > 1);

  if (!success || !m_cycleStartPoints.isEmpty()) {
    m_cycleStartPoints.remove(name);
    return nullptr;
  }
  return CSSVariableData::createResolved(tokens, variableData);
}

bool CSSVariableResolver::resolveVariableReference(
    CSSParserTokenRange range,
    Vector<CSSParserToken>& result) {
  range.consumeWhitespace();
  ASSERT(range.peek().type() == IdentToken);
  AtomicString variableName =
      range.consumeIncludingWhitespace().value().toAtomicString();
  ASSERT(range.atEnd() || (range.peek().type() == CommaToken));

  CSSVariableData* variableData = valueForCustomProperty(variableName);
  if (!variableData)
    return resolveFallback(range, result);

  result.appendVector(variableData->tokens());
  Vector<CSSParserToken> trash;
  resolveFallback(range, trash);
  return true;
}

void CSSVariableResolver::resolveApplyAtRule(CSSParserTokenRange& range,
                                             Vector<CSSParserToken>& result) {
  DCHECK(range.peek().type() == AtKeywordToken &&
         equalIgnoringASCIICase(range.peek().value(), "apply"));
  range.consumeIncludingWhitespace();
  const CSSParserToken& variableName = range.consumeIncludingWhitespace();
  // TODO(timloh): Should we actually be consuming this?
  if (range.peek().type() == SemicolonToken)
    range.consume();

  CSSVariableData* variableData =
      valueForCustomProperty(variableName.value().toAtomicString());
  if (!variableData)
    return;  // Invalid custom property

  CSSParserTokenRange rule = variableData->tokenRange();
  rule.consumeWhitespace();
  if (rule.peek().type() != LeftBraceToken)
    return;
  CSSParserTokenRange ruleContents = rule.consumeBlock();
  rule.consumeWhitespace();
  if (!rule.atEnd())
    return;

  result.appendRange(ruleContents.begin(), ruleContents.end());
}

bool CSSVariableResolver::resolveTokenRange(CSSParserTokenRange range,
                                            Vector<CSSParserToken>& result) {
  bool success = true;
  while (!range.atEnd()) {
    if (range.peek().functionId() == CSSValueVar) {
      success &= resolveVariableReference(range.consumeBlock(), result);
    } else if (range.peek().type() == AtKeywordToken &&
               equalIgnoringASCIICase(range.peek().value(), "apply") &&
               RuntimeEnabledFeatures::cssApplyAtRulesEnabled()) {
      resolveApplyAtRule(range, result);
    } else {
      result.append(range.consume());
    }
  }
  return success;
}

const CSSValue* CSSVariableResolver::resolveVariableReferences(
    const StyleResolverState& state,
    CSSPropertyID id,
    const CSSValue& value) {
  ASSERT(!isShorthandProperty(id));

  if (value.isPendingSubstitutionValue())
    return resolvePendingSubstitutions(state, id,
                                       toCSSPendingSubstitutionValue(value));

  if (value.isVariableReferenceValue())
    return resolveVariableReferences(state, id,
                                     toCSSVariableReferenceValue(value));

  NOTREACHED();
  return nullptr;
}

const CSSValue* CSSVariableResolver::resolveVariableReferences(
    const StyleResolverState& state,
    CSSPropertyID id,
    const CSSVariableReferenceValue& value) {
  CSSVariableResolver resolver(state);
  Vector<CSSParserToken> tokens;
  if (!resolver.resolveTokenRange(value.variableDataValue()->tokens(), tokens))
    return CSSUnsetValue::create();
  const CSSValue* result =
      CSSPropertyParser::parseSingleValue(id, tokens, strictCSSParserContext());
  if (!result)
    return CSSUnsetValue::create();
  return result;
}

const CSSValue* CSSVariableResolver::resolvePendingSubstitutions(
    const StyleResolverState& state,
    CSSPropertyID id,
    const CSSPendingSubstitutionValue& pendingValue) {
  // Longhands from shorthand references follow this path.
  HeapHashMap<CSSPropertyID, Member<const CSSValue>>& propertyCache =
      state.parsedPropertiesForPendingSubstitutionCache(pendingValue);

  const CSSValue* value = propertyCache.get(id);
  if (!value) {
    // TODO(timloh): We shouldn't retry this for all longhands if the shorthand ends up invalid
    CSSVariableReferenceValue* shorthandValue = pendingValue.shorthandValue();
    CSSPropertyID shorthandPropertyId = pendingValue.shorthandPropertyId();

    CSSVariableResolver resolver(state);

    Vector<CSSParserToken> tokens;
    if (resolver.resolveTokenRange(
            shorthandValue->variableDataValue()->tokens(), tokens)) {
      CSSParserContext context(HTMLStandardMode, 0);

      HeapVector<CSSProperty, 256> parsedProperties;

      if (CSSPropertyParser::parseValue(
              shorthandPropertyId, false, CSSParserTokenRange(tokens), context,
              parsedProperties, StyleRule::RuleType::Style)) {
        unsigned parsedPropertiesCount = parsedProperties.size();
        for (unsigned i = 0; i < parsedPropertiesCount; ++i) {
          propertyCache.set(parsedProperties[i].id(),
                            parsedProperties[i].value());
        }
      }
    }
    value = propertyCache.get(id);
  }

  if (value)
    return value;

  return CSSUnsetValue::create();
}

void CSSVariableResolver::resolveVariableDefinitions(
    const StyleResolverState& state) {
  StyleInheritedVariables* inheritedVariables =
      state.style()->inheritedVariables();
  StyleNonInheritedVariables* nonInheritedVariables =
      state.style()->nonInheritedVariables();
  if (!inheritedVariables && !nonInheritedVariables)
    return;

  CSSVariableResolver resolver(state);
  if (inheritedVariables) {
    for (auto& variable : inheritedVariables->m_data)
      resolver.valueForCustomProperty(variable.key);
  }
  if (nonInheritedVariables) {
    for (auto& variable : nonInheritedVariables->m_data)
      resolver.valueForCustomProperty(variable.key);
  }
}

CSSVariableResolver::CSSVariableResolver(const StyleResolverState& state)
    : m_styleResolverState(state),
      m_inheritedVariables(state.style()->inheritedVariables()),
      m_nonInheritedVariables(state.style()->nonInheritedVariables()),
      m_registry(state.document().propertyRegistry()) {}

DEFINE_TRACE(CSSVariableResolver) {
  visitor->trace(m_registry);
}

}  // namespace blink

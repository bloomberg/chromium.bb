// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/CSSVariableResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSValuePool.h"
#include "core/css/CSSVariableData.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/StyleVariableData.h"
#include "wtf/Vector.h"

namespace blink {

bool CSSVariableResolver::resolveFallback(CSSParserTokenRange range, Vector<CSSParserToken>& result)
{
    if (range.atEnd())
        return false;
    ASSERT(range.peek().type() == CommaToken);
    range.consume();
    return resolveVariableReferencesFromTokens(range, result);
}

bool CSSVariableResolver::resolveVariableTokensRecursive(CSSParserTokenRange range,
    Vector<CSSParserToken>& result)
{
    Vector<CSSParserToken> trash;
    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    AtomicString variableName = range.consumeIncludingWhitespace().value();
    ASSERT(range.atEnd() || (range.peek().type() == CommaToken));

    // Cycle detection.
    if (m_variablesSeen.contains(variableName)) {
        m_cycleStartPoints.add(variableName);
        resolveFallback(range, trash);
        return false;
    }

    CSSVariableData* variableData = m_styleVariableData ? m_styleVariableData->getVariable(variableName) : nullptr;
    if (variableData) {
        Vector<CSSParserToken> tokens;
        if (variableData->needsVariableResolution()) {
            m_variablesSeen.add(variableName);
            bool referenceValid = resolveVariableReferencesFromTokens(variableData->tokens(), tokens);
            m_variablesSeen.remove(variableName);

            // The old variable data holds onto the backing string the new resolved CSSVariableData
            // relies on. Ensure it will live beyond us overwriting the RefPtr in StyleVariableData.
            ASSERT(variableData->refCount() > 1);

            if (!referenceValid || !m_cycleStartPoints.isEmpty()) {
                m_styleVariableData->setVariable(variableName, nullptr);
                m_cycleStartPoints.remove(variableName);
                if (!m_cycleStartPoints.isEmpty()) {
                    resolveFallback(range, trash);
                    return false;
                }
                return resolveFallback(range, result);
            }

            m_styleVariableData->setVariable(variableName, CSSVariableData::createResolved(tokens, variableData));
        } else {
            tokens = variableData->tokens();
        }

        ASSERT(!tokens.isEmpty());
        // Check that loops are not induced by the fallback.
        resolveFallback(range, trash);
        if (m_cycleStartPoints.isEmpty()) {
            // It's OK if the fallback fails to resolve - we're not actually taking it.
            result.appendVector(tokens);
            return true;
        }
        return false;
    }

    return resolveFallback(range, result);
}

bool CSSVariableResolver::resolveVariableReferencesFromTokens(CSSParserTokenRange range,
    Vector<CSSParserToken>& result)
{
    bool success = true;
    while (!range.atEnd()) {
        if (range.peek().functionId() == CSSValueVar) {
            success &= resolveVariableTokensRecursive(range.consumeBlock(), result);
        } else {
            result.append(range.consume());
        }
    }
    return success;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSVariableResolver::resolveVariableReferences(StyleVariableData* styleVariableData, CSSPropertyID id, const CSSVariableReferenceValue& value)
{
    ASSERT(!isShorthandProperty(id));

    CSSVariableResolver resolver(styleVariableData);
    Vector<CSSParserToken> tokens;
    if (!resolver.resolveVariableReferencesFromTokens(value.variableDataValue()->tokens(), tokens))
        return cssValuePool().createUnsetValue();

    CSSParserContext context(HTMLStandardMode, nullptr);
    WillBeHeapVector<CSSProperty, 256> parsedProperties;
    // TODO(timloh): This should be CSSParser::parseSingleValue and not need a vector.
    if (!CSSPropertyParser::parseValue(id, false, CSSParserTokenRange(tokens), context, parsedProperties, StyleRule::Type::Style))
        return cssValuePool().createUnsetValue();
    ASSERT(parsedProperties.size() == 1);
    return parsedProperties[0].value();
}

void CSSVariableResolver::resolveAndApplyVariableReferences(StyleResolverState& state, CSSPropertyID id, const CSSVariableReferenceValue& value)
{

    // TODO(leviw): This should be a stack
    CSSVariableResolver resolver(state.style()->variables());

    Vector<CSSParserToken> tokens;
    if (resolver.resolveVariableReferencesFromTokens(value.variableDataValue()->tokens(), tokens)) {
        CSSParserContext context(HTMLStandardMode, 0);

        WillBeHeapVector<CSSProperty, 256> parsedProperties;

        if (CSSPropertyParser::parseValue(id, false, CSSParserTokenRange(tokens), context, parsedProperties, StyleRule::Type::Style)) {
            unsigned parsedPropertiesCount = parsedProperties.size();
            for (unsigned i = 0; i < parsedPropertiesCount; ++i)
                StyleBuilder::applyProperty(parsedProperties[i].id(), state, parsedProperties[i].value());
            return;
        }
    }

    RefPtrWillBeRawPtr<CSSUnsetValue> unset = cssValuePool().createUnsetValue();
    if (isShorthandProperty(id)) {
        StylePropertyShorthand shorthand = shorthandForProperty(id);
        for (unsigned i = 0; i < shorthand.length(); i++)
            StyleBuilder::applyProperty(shorthand.properties()[i], state, unset.get());
        return;
    }

    StyleBuilder::applyProperty(id, state, unset.get());
}

void CSSVariableResolver::resolveVariableDefinitions(StyleVariableData* variables)
{
    if (!variables)
        return;

    for (auto& variable : variables->m_data) {
        if (!variable.value || !variable.value->needsVariableResolution())
            continue;
        Vector<CSSParserToken> resolvedTokens;

        CSSVariableResolver resolver(variables, variable.key);
        if (resolver.resolveVariableReferencesFromTokens(variable.value->tokens(), resolvedTokens))
            variable.value = CSSVariableData::createResolved(resolvedTokens, variable.value);
        else
            variable.value = nullptr;
    }
}

CSSVariableResolver::CSSVariableResolver(StyleVariableData* styleVariableData)
    : m_styleVariableData(styleVariableData)
{
}

CSSVariableResolver::CSSVariableResolver(StyleVariableData* styleVariableData, AtomicString& variable)
    : m_styleVariableData(styleVariableData)
{
    m_variablesSeen.add(variable);
}

} // namespace blink

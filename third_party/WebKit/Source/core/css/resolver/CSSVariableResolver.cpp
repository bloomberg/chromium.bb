// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/resolver/CSSVariableResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
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

bool CSSVariableResolver::resolveVariableTokensRecursive(CSSParserTokenRange range,
    Vector<CSSParserToken>& result)
{
    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    AtomicString variableName = range.consumeIncludingWhitespace().value();
    ASSERT(range.atEnd() || (range.peek().type() == CommaToken));

    if (m_variablesSeen.contains(variableName)) {
        m_cycleDetected = true;
        return false;
    }

    CSSVariableData* variableData = m_styleVariableData ? m_styleVariableData->getVariable(variableName) : nullptr;
    if (variableData) {
        Vector<CSSParserToken> tokens;
        if (variableData->needsVariableResolution()) {
            m_variablesSeen.add(variableName);
            resolveVariableReferencesFromTokens(variableData->tokens(), tokens);
            m_variablesSeen.remove(variableName);

            // The old variable data holds onto the backing string the new resolved CSSVariableData
            // relies on. Ensure it will live beyond us overwriting the RefPtr in StyleVariableData.
            ASSERT(variableData->refCount() > 1);

            m_styleVariableData->setVariable(variableName, CSSVariableData::createResolved(tokens));
        } else {
            tokens = variableData->tokens();
        }
        if (!tokens.isEmpty()) {
            result.appendVector(tokens);
            return true;
        }
    }

    if (range.atEnd())
        return false;

    // Comma Token, as asserted above.
    range.consume();

    resolveVariableReferencesFromTokens(range, result);
    return true;
}

bool CSSVariableResolver::resolveVariableReferencesFromTokens(CSSParserTokenRange range,
    Vector<CSSParserToken>& result)
{
    while (!range.atEnd()) {
        if (range.peek().functionId() != CSSValueVar) {
            result.append(range.consume());
        } else if (!resolveVariableTokensRecursive(range.consumeBlock(), result) || m_cycleDetected) {
            result.clear();
            return false;
        }
    }
    return true;
}

void CSSVariableResolver::resolveAndApplyVariableReferences(StyleResolverState& state, CSSPropertyID id, const CSSVariableReferenceValue& value)
{
    // TODO(leviw): This should be a stack
    CSSVariableResolver resolver(state.style()->variables());

    Vector<CSSParserToken> tokens;
    if (!resolver.resolveVariableReferencesFromTokens(value.variableDataValue()->tokens(), tokens))
        return;

    CSSParserContext context(HTMLStandardMode, 0);

    WillBeHeapVector<CSSProperty, 256> parsedProperties;

    CSSPropertyParser::parseValue(id, false, CSSParserTokenRange(tokens), context, parsedProperties, StyleRule::Type::Style);

    unsigned parsedPropertiesCount = parsedProperties.size();
    for (unsigned i = 0; i < parsedPropertiesCount; ++i)
        StyleBuilder::applyProperty(parsedProperties[i].id(), state, parsedProperties[i].value());
}

void CSSVariableResolver::resolveVariableDefinitions(StyleVariableData* variables)
{
    if (!variables)
        return;

    for (auto& variable : variables->m_data) {
        if (!variable.value->needsVariableResolution())
            continue;
        Vector<CSSParserToken> resolvedTokens;

        CSSVariableResolver resolver(variables, variable.key);
        resolver.resolveVariableReferencesFromTokens(variable.value->tokens(), resolvedTokens);

        variable.value = CSSVariableData::createResolved(resolvedTokens);
    }
}

CSSVariableResolver::CSSVariableResolver(StyleVariableData* styleVariableData)
    : m_styleVariableData(styleVariableData)
    , m_cycleDetected(false)
{
}

CSSVariableResolver::CSSVariableResolver(StyleVariableData* styleVariableData, AtomicString& variable)
    : m_styleVariableData(styleVariableData)
    , m_cycleDetected(false)
{
    m_variablesSeen.add(variable);
}

} // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSSyntaxDescriptor.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"

namespace blink {

CSSSyntaxDescriptor::CSSSyntaxDescriptor(String input)
{
    // TODO(timloh): Implement proper parsing
    if (input.contains('*'))
        m_syntaxComponents.append(CSSSyntaxComponent(CSSSyntaxType::TokenStream));
    else
        m_syntaxComponents.append(CSSSyntaxComponent(CSSSyntaxType::Length));
}

const CSSValue* consumeSingleType(const CSSSyntaxComponent& syntax, CSSParserTokenRange& range)
{
    using namespace CSSPropertyParserHelpers;

    // TODO(timloh): Calc values need to be normalized
    switch (syntax.m_type) {
    case CSSSyntaxType::Length:
        return consumeLength(range, HTMLStandardMode, ValueRange::ValueRangeAll);
    default:
        NOTREACHED();
        return nullptr;
    }
}

const CSSValue* consumeSyntaxComponent(const CSSSyntaxComponent& syntax, CSSParserTokenRange range)
{
    // CSS-wide keywords are already handled by the CSSPropertyParser
    const CSSValue* result = consumeSingleType(syntax, range);
    if (!range.atEnd())
        return nullptr;
    return result;
}

const CSSValue* CSSSyntaxDescriptor::parse(const String& value) const
{
    CSSTokenizer::Scope scope(value);
    return parse(scope.tokenRange());
}

const CSSValue* CSSSyntaxDescriptor::parse(CSSParserTokenRange range) const
{
    if (isTokenStream())
        return CSSVariableParser::parseRegisteredPropertyValue(range, false);
    range.consumeWhitespace();
    for (const CSSSyntaxComponent& component : m_syntaxComponents) {
        if (const CSSValue* result = consumeSyntaxComponent(component, range))
            return result;
    }
    return CSSVariableParser::parseRegisteredPropertyValue(range, true);
}

} // namespace blink

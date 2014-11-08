// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserImpl.h"

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSTokenizer.h"

namespace blink {

CSSParserImpl::CSSParserImpl(const CSSParserContext& context, const String& s)
: m_context(context)
{
    CSSTokenizer::tokenize(s, m_tokens);
}

bool CSSParserImpl::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSRuleSourceData::Type ruleType = CSSRuleSourceData::STYLE_RULE;
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = CSSRuleSourceData::VIEWPORT_RULE;
    parser.consumeDeclarationValue(parser.m_tokens.begin(), parser.m_tokens.end() - 1, propertyID, important, ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;
    declaration->addParsedProperties(parser.m_parsedProperties);
    return true;
}

void CSSParserImpl::consumeDeclarationValue(CSSParserTokenIterator start, CSSParserTokenIterator end, CSSPropertyID propertyID, bool important, CSSRuleSourceData::Type ruleType)
{
    CSSParserValueList valueList(start, end);
    if (!valueList.size())
        return; // Parser error
    bool inViewport = ruleType == CSSRuleSourceData::VIEWPORT_RULE;
    CSSPropertyParser::parseValue(propertyID, important, &valueList, m_context, inViewport, m_parsedProperties, ruleType);
}

} // namespace blink

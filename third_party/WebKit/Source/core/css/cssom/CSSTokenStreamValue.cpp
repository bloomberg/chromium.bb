// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTokenStreamValue.h"

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/parser/CSSTokenizer.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

class TokenStreamValueIterationSource final : public ValueIterable<StringOrCSSVariableReferenceValue>::IterationSource {
public:
    explicit TokenStreamValueIterationSource(CSSTokenStreamValue* tokenStreamValue)
        : m_tokenStreamValue(tokenStreamValue)
    {
    }

    bool next(ScriptState*, StringOrCSSVariableReferenceValue& value, ExceptionState&) override
    {
        if (m_index >= m_tokenStreamValue->size())
            return false;
        value = m_tokenStreamValue->fragmentAtIndex(m_index);
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_tokenStreamValue);
        ValueIterable<StringOrCSSVariableReferenceValue>::IterationSource::trace(visitor);
    }

private:
    const Member<CSSTokenStreamValue> m_tokenStreamValue;
};

StringView findVariableName(CSSParserTokenRange& range)
{
    range.consumeWhitespace();
    return range.consume().value();
}

StringOrCSSVariableReferenceValue variableReferenceValue(const StringView& variableName, const HeapVector<StringOrCSSVariableReferenceValue>& fragments)
{
    CSSTokenStreamValue* tokenStreamValue;
    if (fragments.size() == 0)
        tokenStreamValue = nullptr;
    else
        tokenStreamValue = CSSTokenStreamValue::create(fragments);
    CSSStyleVariableReferenceValue* variableReference = CSSStyleVariableReferenceValue::create(variableName.toString(), tokenStreamValue);
    return StringOrCSSVariableReferenceValue::fromCSSVariableReferenceValue(variableReference);
}

HeapVector<StringOrCSSVariableReferenceValue> parserTokenRangeToFragments(CSSParserTokenRange range)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    StringBuilder builder;
    while (!range.atEnd()) {
        if (range.peek().functionId() == CSSValueVar) {
            if (!builder.isEmpty()) {
                fragments.append(StringOrCSSVariableReferenceValue::fromString(builder.toString()));
                builder.clear();
            }
            CSSParserTokenRange block = range.consumeBlock();
            StringView variableName = findVariableName(block);
            block.consumeWhitespace();
            if (block.peek().type() == CSSParserTokenType::CommaToken)
                block.consume();
            fragments.append(variableReferenceValue(variableName, parserTokenRangeToFragments(block)));
        } else {
            range.consume().serialize(builder);
        }
    }
    if (!builder.isEmpty())
        fragments.append(StringOrCSSVariableReferenceValue::fromString(builder.toString()));
    return fragments;
}

} // namespace

ValueIterable<StringOrCSSVariableReferenceValue>::IterationSource* CSSTokenStreamValue::startIteration(ScriptState*, ExceptionState&)
{
    return new TokenStreamValueIterationSource(this);
}

CSSTokenStreamValue* CSSTokenStreamValue::fromCSSValue(const CSSVariableReferenceValue& cssVariableReferenceValue)
{
    return CSSTokenStreamValue::create(parserTokenRangeToFragments(cssVariableReferenceValue.variableDataValue()->tokenRange()));
}

CSSValue* CSSTokenStreamValue::toCSSValue() const
{
    StringBuilder tokens;

    for (unsigned i = 0; i < m_fragments.size(); i++) {
        if (i)
            tokens.append("/**/");
        if (m_fragments[i].isString())
            tokens.append(m_fragments[i].getAsString());
        else if (m_fragments[i].isCSSVariableReferenceValue())
            tokens.append(m_fragments[i].getAsCSSVariableReferenceValue()->variable());
        else
            NOTREACHED();
    }

    CSSTokenizer::Scope scope(tokens.toString());

    return CSSVariableReferenceValue::create(CSSVariableData::create(scope.tokenRange()));
}

} // namespace blink

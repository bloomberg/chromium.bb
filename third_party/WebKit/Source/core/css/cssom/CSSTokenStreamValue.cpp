// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTokenStreamValue.h"

#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSTokenizer.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

class TokenStreamValueIterationSource final : public ValueIterable<String>::IterationSource {
public:
    explicit TokenStreamValueIterationSource(CSSTokenStreamValue* tokenStreamValue)
        : m_tokenStreamValue(tokenStreamValue)
    {
    }

    bool next(ScriptState*, String& value, ExceptionState&) override
    {
        if (m_index >= m_tokenStreamValue->size())
            return false;
        value = m_tokenStreamValue->fragmentAtIndex(m_index);
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_tokenStreamValue);
        ValueIterable<String>::IterationSource::trace(visitor);
    }

private:
    const Member<CSSTokenStreamValue> m_tokenStreamValue;
};

} // namespace

ValueIterable<String>::IterationSource* CSSTokenStreamValue::startIteration(ScriptState*, ExceptionState&)
{
    return new TokenStreamValueIterationSource(this);
}

CSSValue* CSSTokenStreamValue::toCSSValue() const
{
    StringBuilder tokens;

    for (unsigned i = 0; i < m_fragments.size(); i++) {
        if (i)
            tokens.append("/**/");
        tokens.append(m_fragments[i]);
    }

    CSSTokenizer::Scope scope(tokens.toString());

    return CSSVariableReferenceValue::create(CSSVariableData::create(scope.tokenRange()));
}

} // namespace blink

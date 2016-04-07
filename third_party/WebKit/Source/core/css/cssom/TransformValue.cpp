// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/TransformValue.h"

#include "core/css/CSSValueList.h"
#include "core/css/cssom/TransformComponent.h"

namespace blink {

namespace {

class TransformValueIterationSource final : public ValueIterable<TransformComponent*>::IterationSource {
public:
    explicit TransformValueIterationSource(TransformValue* transformValue)
        : m_transformValue(transformValue)
    {
    }

    bool next(ScriptState*, TransformComponent*& value, ExceptionState&) override
    {
        if (m_index >= m_transformValue->size()) {
            return false;
        }
        value = m_transformValue->componentAtIndex(m_index);
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_transformValue);
        ValueIterable<TransformComponent*>::IterationSource::trace(visitor);
    }

private:
    const Member<TransformValue> m_transformValue;
};

} // namespace

ValueIterable<TransformComponent*>::IterationSource* TransformValue::startIteration(ScriptState*, ExceptionState&)
{
    return new TransformValueIterationSource(this);
}

bool TransformValue::is2D() const
{
    for (size_t i = 0; i < m_transformComponents.size(); i++) {
        if (!m_transformComponents[i]->is2DComponent()) {
            return false;
        }
    }
    return true;
}

CSSValue* TransformValue::toCSSValue() const
{
    CSSValueList* transformCSSValue = CSSValueList::createSpaceSeparated();
    for (size_t i = 0; i < m_transformComponents.size(); i++) {
        transformCSSValue->append(m_transformComponents[i]->toCSSValue());
    }
    return transformCSSValue;
}

} // namespace blink

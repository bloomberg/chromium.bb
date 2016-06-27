// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/TransformValue.h"

#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

namespace {

class TransformValueIterationSource final : public ValueIterable<CSSTransformComponent*>::IterationSource {
public:
    explicit TransformValueIterationSource(TransformValue* transformValue)
        : m_transformValue(transformValue)
    {
    }

    bool next(ScriptState*, CSSTransformComponent*& value, ExceptionState&) override
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
        ValueIterable<CSSTransformComponent*>::IterationSource::trace(visitor);
    }

private:
    const Member<TransformValue> m_transformValue;
};

} // namespace

TransformValue* TransformValue::fromCSSValue(const CSSValue& cssValue)
{
    if (!cssValue.isValueList()) {
        // TODO(meade): Also need to check the separator here if we care.
        return nullptr;
    }
    HeapVector<Member<CSSTransformComponent>> components;
    for (const CSSValue* value : toCSSValueList(cssValue)) {
        CSSTransformComponent* component = CSSTransformComponent::fromCSSValue(*value);
        if (!component)
            return nullptr;
        components.append(component);
    }
    return TransformValue::create(components);
}

ValueIterable<CSSTransformComponent*>::IterationSource* TransformValue::startIteration(ScriptState*, ExceptionState&)
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
        transformCSSValue->append(*m_transformComponents[i]->toCSSValue());
    }
    return transformCSSValue;
}

} // namespace blink

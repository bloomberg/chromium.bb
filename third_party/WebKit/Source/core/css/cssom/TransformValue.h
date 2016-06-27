// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformValue_h
#define TransformValue_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSTransformComponent.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class CORE_EXPORT TransformValue final : public CSSStyleValue, public ValueIterable<CSSTransformComponent*> {
    WTF_MAKE_NONCOPYABLE(TransformValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static TransformValue* create()
    {
        return new TransformValue();
    }

    static TransformValue* create(const HeapVector<Member<CSSTransformComponent>>& transformComponents)
    {
        return new TransformValue(transformComponents);
    }

    static TransformValue* fromCSSValue(const CSSValue&);

    bool is2D() const;

    CSSValue* toCSSValue() const override;

    StyleValueType type() const override { return TransformType; }

    CSSTransformComponent* componentAtIndex(int index) { return m_transformComponents.at(index); }

    size_t size() { return m_transformComponents.size(); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_transformComponents);
        CSSStyleValue::trace(visitor);
    }

private:
    TransformValue() {}
    TransformValue(const HeapVector<Member<CSSTransformComponent>>& transformComponents) : CSSStyleValue(),
        m_transformComponents(transformComponents) {}

    HeapVector<Member<CSSTransformComponent>> m_transformComponents;

    IterationSource* startIteration(ScriptState*, ExceptionState&) override;
};

} // namespace blink

#endif

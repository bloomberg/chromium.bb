// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformValue_h
#define TransformValue_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/StyleValue.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class TransformComponent;

class CORE_EXPORT TransformValue final : public StyleValue, public ValueIterable<TransformComponent*> {
    WTF_MAKE_NONCOPYABLE(TransformValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static TransformValue* create()
    {
        return new TransformValue();
    }

    static TransformValue* create(const HeapVector<Member<TransformComponent>>& transformComponents)
    {
        return new TransformValue(transformComponents);
    }

    bool is2D() const;

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

    StyleValueType type() const override { return TransformValueType; }

    TransformComponent* componentAtIndex(int index) { return m_transformComponents.at(index); }

    size_t size() { return m_transformComponents.size(); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_transformComponents);
        StyleValue::trace(visitor);
    }

private:
    TransformValue() {}
    TransformValue(const HeapVector<Member<TransformComponent>>& transformComponents) : StyleValue(),
        m_transformComponents(transformComponents) {}

    HeapVector<Member<TransformComponent>> m_transformComponents;

    IterationSource* startIteration(ScriptState*, ExceptionState&) override;
};

} // namespace blink

#endif

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingValue_h
#define UnderlyingValue_h

#include "core/animation/InterpolationValue.h"
#include "wtf/Allocator.h"

namespace blink {

// Handles memory management of underlying InterpolationValues in applyStack()
// Ensures we perform copy on write if we are not the owner of an underlying InterpolationValue.
// This functions similar to a DataRef except on OwnPtr'd objects.
class UnderlyingValue {
    STACK_ALLOCATED();

public:
    UnderlyingValue()
        : m_valueOwner(nullptr)
        , m_value(nullptr)
    { }

    void set(const InterpolationValue* interpolationValue)
    {
        // By clearing m_valueOwner we will perform a copy before attempting to mutate m_value,
        // thus upholding the const contract for this instance of interpolationValue despite the const_cast.
        m_valueOwner.clear();
        m_value = const_cast<InterpolationValue*>(interpolationValue);
    }
    void set(PassOwnPtr<InterpolationValue> interpolationValue)
    {
        m_valueOwner = std::move(interpolationValue);
        m_value = m_valueOwner.get();
    }
    InterpolationComponent& mutableComponent()
    {
        ASSERT(m_value);
        if (!m_valueOwner)
            set(m_value->clone());
        return m_value->mutableComponent();
    }
    const InterpolationValue* get() const { return m_value; }
    operator bool() const { return m_value; }
    const InterpolationValue* operator->() const
    {
        ASSERT(m_value);
        return m_value;
    }

private:
    OwnPtr<InterpolationValue> m_valueOwner;
    InterpolationValue* m_value;
};

} // namespace blink

#endif // UnderlyingValue_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationValue_h
#define InterpolationValue_h

#include "core/animation/InterpolationComponent.h"

namespace blink {

class InterpolationType;

class InterpolationValue {
public:
    static PassOwnPtr<InterpolationValue> create(const InterpolationType& type, InterpolationComponent& component)
    {
        return adoptPtr(new InterpolationValue(type, component.interpolableValue.release(), component.nonInterpolableValue.release()));
    }
    static PassOwnPtr<InterpolationValue> create(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
    {
        return adoptPtr(new InterpolationValue(type, std::move(interpolableValue), nonInterpolableValue));
    }

    PassOwnPtr<InterpolationValue> clone() const
    {
        return create(m_type, m_component.interpolableValue->clone(), m_component.nonInterpolableValue);
    }

    const InterpolationType& type() const { return m_type; }
    const InterpolableValue& interpolableValue() const { return *m_component.interpolableValue; }
    const NonInterpolableValue* nonInterpolableValue() const { return m_component.nonInterpolableValue.get(); }
    const InterpolationComponent& component() const { return m_component; }

    InterpolationComponent& mutableComponent() { return m_component; }

private:
    InterpolationValue(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue)
        : m_type(type)
        , m_component(std::move(interpolableValue), nonInterpolableValue)
    {
        ASSERT(m_component.interpolableValue);
    }

    const InterpolationType& m_type;
    InterpolationComponent m_component;
};

} // namespace blink

#endif // InterpolationValue_h

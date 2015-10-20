// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationValue_h
#define InterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class InterpolationType;

struct InterpolationComponentValue {
    ALLOW_ONLY_INLINE_ALLOCATION();

    explicit InterpolationComponentValue(PassOwnPtr<InterpolableValue> interpolableValue = nullptr, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : interpolableValue(interpolableValue)
        , nonInterpolableValue(nonInterpolableValue)
    { }

    InterpolationComponentValue(const void* null) { ASSERT(null == 0); }

    InterpolationComponentValue(InterpolationComponentValue&& other)
        : interpolableValue(other.interpolableValue.release())
        , nonInterpolableValue(other.nonInterpolableValue.release())
    { }

    operator bool() const { return interpolableValue; }

    OwnPtr<InterpolableValue> interpolableValue;
    RefPtr<NonInterpolableValue> nonInterpolableValue;
};

class InterpolationValue {
public:
    static PassOwnPtr<InterpolationValue> create(const InterpolationType& type, InterpolationComponentValue& component)
    {
        return adoptPtr(new InterpolationValue(type, component.interpolableValue.release(), component.nonInterpolableValue.release()));
    }
    static PassOwnPtr<InterpolationValue> create(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
    {
        return adoptPtr(new InterpolationValue(type, interpolableValue, nonInterpolableValue));
    }

    PassOwnPtr<InterpolationValue> clone() const
    {
        return create(m_type, m_component.interpolableValue->clone(), m_component.nonInterpolableValue);
    }

    const InterpolationType& type() const { return m_type; }
    const InterpolableValue& interpolableValue() const { return *m_component.interpolableValue; }
    const NonInterpolableValue* nonInterpolableValue() const { return m_component.nonInterpolableValue.get(); }

    InterpolationComponentValue& mutableComponent() { return m_component; }

private:
    InterpolationValue(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue)
        : m_type(type)
        , m_component(interpolableValue, nonInterpolableValue)
    {
        ASSERT(m_component.interpolableValue);
    }

    const InterpolationType& m_type;
    InterpolationComponentValue m_component;
};

} // namespace blink

#endif // InterpolationValue_h

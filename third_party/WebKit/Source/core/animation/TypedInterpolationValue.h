// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TypedInterpolationValue_h
#define TypedInterpolationValue_h

#include "core/animation/InterpolationValue.h"

namespace blink {

class InterpolationType;

// Represents an interpolated value between an adjacent pair of PropertySpecificKeyframes.
class TypedInterpolationValue {
public:
    static PassOwnPtr<TypedInterpolationValue> create(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
    {
        return adoptPtr(new TypedInterpolationValue(type, std::move(interpolableValue), nonInterpolableValue));
    }

    PassOwnPtr<TypedInterpolationValue> clone() const
    {
        InterpolationValue copy = m_value.clone();
        return create(m_type, copy.interpolableValue.release(), copy.nonInterpolableValue.release());
    }

    const InterpolationType& type() const { return m_type; }
    const InterpolableValue& interpolableValue() const { return *m_value.interpolableValue; }
    const NonInterpolableValue* getNonInterpolableValue() const { return m_value.nonInterpolableValue.get(); }
    const InterpolationValue& value() const { return m_value; }

    InterpolationValue& mutableValue() { return m_value; }

private:
    TypedInterpolationValue(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue)
        : m_type(type)
        , m_value(std::move(interpolableValue), nonInterpolableValue)
    {
        ASSERT(m_value.interpolableValue);
    }

    const InterpolationType& m_type;
    InterpolationValue m_value;
};

} // namespace blink

#endif // TypedInterpolationValue_h

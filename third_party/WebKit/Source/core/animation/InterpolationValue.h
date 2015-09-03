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

class InterpolationValue {
public:
    static PassOwnPtr<InterpolationValue> create(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
    {
        return adoptPtr(new InterpolationValue(type, interpolableValue, nonInterpolableValue));
    }

    PassOwnPtr<InterpolationValue> clone() const
    {
        return create(m_type, m_interpolableValue->clone(), m_nonInterpolableValue);
    }

    const InterpolationType& type() const { return m_type; }
    const InterpolableValue& interpolableValue() const { return *m_interpolableValue; }
    InterpolableValue& interpolableValue() { return *m_interpolableValue; }
    const NonInterpolableValue* nonInterpolableValue() const { return m_nonInterpolableValue.get(); }

private:
    InterpolationValue(const InterpolationType& type, PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : m_type(type)
        , m_interpolableValue(interpolableValue)
        , m_nonInterpolableValue(nonInterpolableValue)
    {
        ASSERT(this->m_interpolableValue);
    }

    const InterpolationType& m_type;
    OwnPtr<InterpolableValue> m_interpolableValue;
    RefPtrWillBePersistent<NonInterpolableValue> m_nonInterpolableValue;

    friend class InterpolationType;
};

} // namespace blink

#endif // InterpolationValue_h

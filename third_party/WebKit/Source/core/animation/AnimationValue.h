// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationValue_h
#define AnimationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class AnimationType;

class AnimationValue : public NoBaseWillBeGarbageCollectedFinalized<AnimationValue> {
public:
    static PassOwnPtrWillBeRawPtr<AnimationValue> create(const AnimationType& type, PassOwnPtrWillBeRawPtr<InterpolableValue> interpolableValue, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
    {
        return adoptPtrWillBeNoop(new AnimationValue(type, interpolableValue, nonInterpolableValue));
    }

    PassOwnPtrWillBeRawPtr<AnimationValue> clone() const
    {
        return create(m_type, m_interpolableValue->clone(), m_nonInterpolableValue);
    }

    const AnimationType& type() const { return m_type; }
    const InterpolableValue& interpolableValue() const { return *m_interpolableValue; }
    InterpolableValue& interpolableValue() { return *m_interpolableValue; }
    const NonInterpolableValue* nonInterpolableValue() const { return m_nonInterpolableValue.get(); }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_interpolableValue);
        visitor->trace(m_nonInterpolableValue);
    }

private:
    AnimationValue(const AnimationType& type, PassOwnPtrWillBeRawPtr<InterpolableValue> interpolableValue, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : m_type(type)
        , m_interpolableValue(interpolableValue)
        , m_nonInterpolableValue(nonInterpolableValue)
    {
        ASSERT(this->m_interpolableValue);
    }

    const AnimationType& m_type;
    OwnPtrWillBeMember<InterpolableValue> m_interpolableValue;
    RefPtrWillBeMember<NonInterpolableValue> m_nonInterpolableValue;

    friend class AnimationType;
};

} // namespace blink

#endif // AnimationValue_h

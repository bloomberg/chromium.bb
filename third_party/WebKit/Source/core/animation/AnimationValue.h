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

struct AnimationValue {
private:
    DISALLOW_ALLOCATION();

public:
    const AnimationType* type;
    OwnPtrWillBeMember<InterpolableValue> interpolableValue;
    RefPtrWillBeMember<NonInterpolableValue> nonInterpolableValue;

    operator bool() const { return bool(type); }

    AnimationValue()
        : type(nullptr)
        , interpolableValue(nullptr)
        , nonInterpolableValue(nullptr)
    { }

    AnimationValue(const AnimationType* type, PassOwnPtrWillBeRawPtr<InterpolableValue> interpolableValue, RefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue)
        : type(type)
        , interpolableValue(interpolableValue)
        , nonInterpolableValue(nonInterpolableValue)
    { }

    void copyFrom(const AnimationValue& other)
    {
        type = other.type;
        interpolableValue = other.interpolableValue ? other.interpolableValue->clone() : nullptr;
        nonInterpolableValue = other.nonInterpolableValue;
    }

    void clear()
    {
        type = nullptr;
        interpolableValue = nullptr;
        nonInterpolableValue = nullptr;
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(interpolableValue);
        visitor->trace(nonInterpolableValue);
    }
};

} // namespace blink

#endif // AnimationValue_h

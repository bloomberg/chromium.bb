// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationValue_h
#define InterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

// Represents a (non-strict) subset of a PropertySpecificKeyframe's value broken down into interpolable and non-interpolable parts.
// InterpolationValues can be composed together to represent a whole PropertySpecificKeyframe value.
struct InterpolationValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    explicit InterpolationValue(PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : interpolableValue(std::move(interpolableValue))
        , nonInterpolableValue(nonInterpolableValue)
    { }

    InterpolationValue(std::nullptr_t) { }

    InterpolationValue(InterpolationValue&& other)
        : interpolableValue(other.interpolableValue.release())
        , nonInterpolableValue(other.nonInterpolableValue.release())
    { }

    void operator=(InterpolationValue&& other)
    {
        interpolableValue = other.interpolableValue.release();
        nonInterpolableValue = other.nonInterpolableValue.release();
    }

    operator bool() const { return interpolableValue; }

    InterpolationValue clone() const
    {
        return InterpolationValue(interpolableValue ? interpolableValue->clone() : nullptr, nonInterpolableValue);
    }

    void clear()
    {
        interpolableValue.clear();
        nonInterpolableValue.clear();
    }

    OwnPtr<InterpolableValue> interpolableValue;
    RefPtr<NonInterpolableValue> nonInterpolableValue;
};

} // namespace blink

#endif // InterpolationValue_h

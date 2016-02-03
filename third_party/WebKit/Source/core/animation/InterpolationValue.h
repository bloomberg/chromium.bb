// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationValue_h
#define InterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

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

struct PairwiseInterpolationValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    PairwiseInterpolationValue(PassOwnPtr<InterpolableValue> startInterpolableValue, PassOwnPtr<InterpolableValue> endInterpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : startInterpolableValue(std::move(startInterpolableValue))
        , endInterpolableValue(std::move(endInterpolableValue))
        , nonInterpolableValue(nonInterpolableValue)
    { }

    PairwiseInterpolationValue(std::nullptr_t) { }

    PairwiseInterpolationValue(PairwiseInterpolationValue&& other)
        : startInterpolableValue(other.startInterpolableValue.release())
        , endInterpolableValue(other.endInterpolableValue.release())
        , nonInterpolableValue(other.nonInterpolableValue.release())
    { }

    operator bool() const { return startInterpolableValue; }

    OwnPtr<InterpolableValue> startInterpolableValue;
    OwnPtr<InterpolableValue> endInterpolableValue;
    RefPtr<NonInterpolableValue> nonInterpolableValue;
};

} // namespace blink

#endif // InterpolationValue_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationComponent_h
#define InterpolationComponent_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

struct InterpolationComponent {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    explicit InterpolationComponent(PassOwnPtr<InterpolableValue> interpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : interpolableValue(std::move(interpolableValue))
        , nonInterpolableValue(std::move(nonInterpolableValue))
    { }

    InterpolationComponent(std::nullptr_t) { }

    InterpolationComponent(InterpolationComponent&& other)
        : interpolableValue(other.interpolableValue.release())
        , nonInterpolableValue(other.nonInterpolableValue.release())
    { }

    operator bool() const { return interpolableValue; }

    OwnPtr<InterpolableValue> interpolableValue;
    RefPtr<NonInterpolableValue> nonInterpolableValue;
};

struct PairwiseInterpolationComponent {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    PairwiseInterpolationComponent(PassOwnPtr<InterpolableValue> startInterpolableValue, PassOwnPtr<InterpolableValue> endInterpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : startInterpolableValue(std::move(startInterpolableValue))
        , endInterpolableValue(std::move(endInterpolableValue))
        , nonInterpolableValue(nonInterpolableValue)
    { }

    PairwiseInterpolationComponent(std::nullptr_t) { }

    PairwiseInterpolationComponent(PairwiseInterpolationComponent&& other)
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

#endif // InterpolationComponent_h

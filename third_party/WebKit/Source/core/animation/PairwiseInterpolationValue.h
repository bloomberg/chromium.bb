// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PairwiseInterpolationValue_h
#define PairwiseInterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

// Represents the smooth interpolation between an adjacent pair of PropertySpecificKeyframes.
struct PairwiseInterpolationValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    PairwiseInterpolationValue(PassOwnPtr<InterpolableValue> startInterpolableValue, PassOwnPtr<InterpolableValue> endInterpolableValue, PassRefPtr<NonInterpolableValue> nonInterpolableValue = nullptr)
        : startInterpolableValue(startInterpolableValue)
        , endInterpolableValue(endInterpolableValue)
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

#endif // PairwiseInterpolationValue_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationCurve.h"

#include "cc/animation/timing_function.h"

namespace blink {

scoped_ptr<cc::TimingFunction> CompositorAnimationCurve::createTimingFunction(TimingFunctionType type)
{
    switch (type) {
    case blink::CompositorAnimationCurve::TimingFunctionTypeEase:
        return cc::EaseTimingFunction::Create();
    case blink::CompositorAnimationCurve::TimingFunctionTypeEaseIn:
        return cc::EaseInTimingFunction::Create();
    case blink::CompositorAnimationCurve::TimingFunctionTypeEaseOut:
        return cc::EaseOutTimingFunction::Create();
    case blink::CompositorAnimationCurve::TimingFunctionTypeEaseInOut:
        return cc::EaseInOutTimingFunction::Create();
    case blink::CompositorAnimationCurve::TimingFunctionTypeLinear:
        return nullptr;
    }
    return nullptr;
}

} // namespace blink

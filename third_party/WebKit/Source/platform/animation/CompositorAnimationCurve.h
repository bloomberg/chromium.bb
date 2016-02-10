// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationCurve_h
#define CompositorAnimationCurve_h

#include "base/memory/scoped_ptr.h"
#include "platform/PlatformExport.h"

namespace cc {
class TimingFunction;
}

namespace blink {

class PLATFORM_EXPORT CompositorAnimationCurve {
public:
    enum TimingFunctionType {
        TimingFunctionTypeEase,
        TimingFunctionTypeEaseIn,
        TimingFunctionTypeEaseOut,
        TimingFunctionTypeEaseInOut,
        TimingFunctionTypeLinear
    };

    enum AnimationCurveType {
        AnimationCurveTypeFilter,
        AnimationCurveTypeFloat,
        AnimationCurveTypeScrollOffset,
        AnimationCurveTypeTransform,
    };

    virtual ~CompositorAnimationCurve() {}

    virtual AnimationCurveType type() const = 0;

protected:
    static scoped_ptr<cc::TimingFunction> createTimingFunction(TimingFunctionType);
};

} // namespace blink

#endif // CompositorAnimationCurve_h

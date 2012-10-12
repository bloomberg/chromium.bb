// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "WebAnimationCurveCommon.h"

#include "CCTimingFunction.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

PassOwnPtr<cc::CCTimingFunction> createTimingFunction(WebAnimationCurve::TimingFunctionType type)
{
    switch (type) {
    case WebAnimationCurve::TimingFunctionTypeEase:
        return cc::CCEaseTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseIn:
        return cc::CCEaseInTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseOut:
        return cc::CCEaseOutTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseInOut:
        return cc::CCEaseInOutTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeLinear:
        return nullptr;
    }
    return nullptr;
}

} // namespace WebKit

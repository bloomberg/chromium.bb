// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "WebAnimationCurveCommon.h"

#include "CCTimingFunction.h"

namespace WebKit {

scoped_ptr<cc::CCTimingFunction> createTimingFunction(WebAnimationCurve::TimingFunctionType type)
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
        return scoped_ptr<cc::CCTimingFunction>();
    }
    return scoped_ptr<cc::CCTimingFunction>();
}

} // namespace WebKit

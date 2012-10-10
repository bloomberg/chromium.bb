// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAnimationCurveCommon_h
#define WebAnimationCurveCommon_h

#include "base/memory/scoped_ptr.h"
#include <public/WebAnimationCurve.h>
#include <wtf/Forward.h>

namespace cc {
class CCTimingFunction;
}

namespace WebKit {
scoped_ptr<cc::CCTimingFunction> createTimingFunction(WebAnimationCurve::TimingFunctionType);
}

#endif // WebAnimationCurveCommon_h

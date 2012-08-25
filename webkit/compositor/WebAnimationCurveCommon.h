// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAnimationCurveCommon_h
#define WebAnimationCurveCommon_h

#include <public/WebAnimationCurve.h>
#include <wtf/Forward.h>

namespace WebCore {
class CCTimingFunction;
}

namespace WebKit {
PassOwnPtr<WebCore::CCTimingFunction> createTimingFunction(WebAnimationCurve::TimingFunctionType);
}

#endif // WebAnimationCurveCommon_h

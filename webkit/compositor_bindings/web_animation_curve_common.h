// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAnimationCurveCommon_h
#define WebAnimationCurveCommon_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationCurve.h"

namespace cc { class TimingFunction; }

namespace WebKit {
scoped_ptr<cc::TimingFunction> createTimingFunction(
    WebAnimationCurve::TimingFunctionType);
}

#endif  // WebAnimationCurveCommon_h

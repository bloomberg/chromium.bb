// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_CURVE_COMMON_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_CURVE_COMMON_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebAnimationCurve.h"

namespace cc { class TimingFunction; }

namespace webkit {
scoped_ptr<cc::TimingFunction> CreateTimingFunction(
    WebKit::WebAnimationCurve::TimingFunctionType);
}

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_CURVE_COMMON_H_

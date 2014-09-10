// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInvalidationDebugAnnotations_h
#define WebInvalidationDebugAnnotations_h

#include "public/platform/WebFloatRect.h"

namespace blink {

enum WebInvalidationDebugAnnotations {
    WebInvalidationDebugAnnotationsNone = 0,
    WebInvalidationDebugAnnotationsFirstPaint = 1 << 0,
    WebInvalidationDebugAnnotationsScrollbar = 1 << 1,
};

struct WebAnnotatedInvalidationRect {
    WebFloatRect rect;
    WebInvalidationDebugAnnotations annotations;
};

} // namespace blink

#endif // WebInvalidationDebugAnnotations_h

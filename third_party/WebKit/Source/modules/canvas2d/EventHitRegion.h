// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventHitRegion_h
#define EventHitRegion_h

#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class HTMLCanvasElement;
class LayoutPoint;

class EventHitRegion {
    STATIC_ONLY(EventHitRegion);
public:
    static String regionIdFromAbsoluteLocation(HTMLCanvasElement&, const LayoutPoint&);
};

} // namespace blink

#endif

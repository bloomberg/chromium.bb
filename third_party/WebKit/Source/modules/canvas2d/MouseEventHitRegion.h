// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MouseEventHitRegion_h
#define MouseEventHitRegion_h

#include "modules/canvas2d/EventHitRegion.h"
#include "wtf/Allocator.h"

namespace blink {

class MouseEvent;

class MouseEventHitRegion : public EventHitRegion {
    STATIC_ONLY(MouseEventHitRegion);
public:
    static String region(MouseEvent&);
};

} // namespace blink

#endif

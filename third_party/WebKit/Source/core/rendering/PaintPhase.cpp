// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/PaintPhase.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

// DisplayItem types must be kept in sync with PaintPhase.
static_assert((unsigned)DisplayItem::DrawingPaintPhaseBlockBackground == (unsigned)PaintPhaseBlockBackground, "DisplayItem Type should stay in sync");
static_assert((unsigned)DisplayItem::DrawingPaintPhaseClippingMask == (unsigned)PaintPhaseClippingMask, "DisplayItem Type should stay in sync");

} // namespace blink

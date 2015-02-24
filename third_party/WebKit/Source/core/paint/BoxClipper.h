// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxClipper_h
#define BoxClipper_h

#include "core/layout/PaintPhase.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class LayoutBox;
struct PaintInfo;

enum ContentsClipBehavior { ForceContentsClip, SkipContentsClipIfPossible };

class BoxClipper {
public:
    BoxClipper(LayoutBox&, const PaintInfo&, const LayoutPoint& accumulatedOffset, ContentsClipBehavior);
    ~BoxClipper();

    bool pushedClip() { return m_pushedClip; }
private:
    bool m_pushedClip;
    LayoutPoint m_accumulatedOffset;
    const PaintInfo& m_paintInfo;
    LayoutBox& m_box;
    DisplayItem::Type m_clipType;
};

} // namespace blink

#endif // BoxClipper_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderBoxClipper_h
#define RenderBoxClipper_h

#include "core/rendering/PaintPhase.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

class RenderBox;
struct PaintInfo;

enum ContentsClipBehavior { ForceContentsClip, SkipContentsClipIfPossible };

class RenderBoxClipper {
public:
    RenderBoxClipper(RenderBox&, PaintInfo&, const LayoutPoint& accumulatedOffset, ContentsClipBehavior);
    ~RenderBoxClipper();

    bool pushedClip() { return m_pushedClip; }
private:
    bool m_pushedClip;
    LayoutPoint m_accumulatedOffset;
    PaintInfo& m_paintInfo;
    PaintPhase m_originalPhase;
    RenderBox& m_box;
};

} // namespace blink

#endif // RenderBoxClipper_h

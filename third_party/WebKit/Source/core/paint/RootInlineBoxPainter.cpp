// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/RootInlineBoxPainter.h"

#include "core/rendering/EllipsisBox.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RootInlineBox.h"

namespace blink {

void RootInlineBoxPainter::paintEllipsisBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom) const
{
    if (m_rootInlineBox.hasEllipsisBox() && paintInfo.shouldPaintWithinRoot(&m_rootInlineBox.renderer()) && m_rootInlineBox.renderer().style()->visibility() == VISIBLE
        && paintInfo.phase == PaintPhaseForeground)
        m_rootInlineBox.ellipsisBox()->paint(paintInfo, paintOffset, lineTop, lineBottom);
}

void RootInlineBoxPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    m_rootInlineBox.InlineFlowBox::paint(paintInfo, paintOffset, lineTop, lineBottom);
    paintEllipsisBox(paintInfo, paintOffset, lineTop, lineBottom);
}

} // namespace blink

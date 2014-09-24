// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ListItemPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderListItem.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void ListItemPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_renderListItem.logicalHeight() && m_renderListItem.hasOverflowClip())
        return;

    BlockPainter(m_renderListItem).paint(paintInfo, paintOffset);
}

} // namespace blink

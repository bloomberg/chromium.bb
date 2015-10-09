// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/PaintArtifact.h"

#include "platform/TraceEvent.h"

namespace blink {

PaintArtifact::PaintArtifact()
    : m_displayItems(0)
{
}

PaintArtifact::~PaintArtifact()
{
}

void PaintArtifact::reset()
{
    m_displayItems.clear();
    m_paintChunks.clear();
}

size_t PaintArtifact::approximateUnsharedMemoryUsage() const
{
    return sizeof(*this) + m_displayItems.memoryUsageInBytes()
        + m_paintChunks.capacity() * sizeof(m_paintChunks[0]);
}

void PaintArtifact::replay(GraphicsContext& graphicsContext) const
{
    TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
    for (const DisplayItem& displayItem : m_displayItems)
        displayItem.replay(graphicsContext);
}

void PaintArtifact::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    TRACE_EVENT0("blink,benchmark", "PaintArtifact::appendToWebDisplayItemList");
    for (const DisplayItem& displayItem : m_displayItems)
        displayItem.appendToWebDisplayItemList(list);
}

} // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintArtifact.h"

#include "platform/TraceEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

namespace {

void computeChunkBounds(const DisplayItemList& displayItems, Vector<PaintChunk>& paintChunks)
{
    for (PaintChunk& chunk : paintChunks) {
        FloatRect bounds;
        for (const DisplayItem& item : displayItems.itemsInPaintChunk(chunk)) {
            if (!item.isDrawing())
                continue;
            const auto& drawing = static_cast<const DrawingDisplayItem&>(item);
            if (const SkPicture* picture = drawing.picture())
                bounds.unite(picture->cullRect());
        }
        chunk.bounds = bounds;
    }
}

} // namespace

PaintArtifact::PaintArtifact()
    : m_displayItemList(0)
{
}

PaintArtifact::PaintArtifact(DisplayItemList displayItems, Vector<PaintChunk> paintChunks)
    : m_displayItemList(std::move(displayItems))
    , m_paintChunks(std::move(paintChunks))
{
    computeChunkBounds(m_displayItemList, m_paintChunks);
}

PaintArtifact::PaintArtifact(PaintArtifact&& source)
    : m_displayItemList(std::move(source.m_displayItemList))
    , m_paintChunks(std::move(source.m_paintChunks))
{
}

PaintArtifact::~PaintArtifact()
{
}

PaintArtifact& PaintArtifact::operator=(PaintArtifact&& source)
{
    m_displayItemList = std::move(source.m_displayItemList);
    m_paintChunks = std::move(source.m_paintChunks);
    return *this;
}

void PaintArtifact::reset()
{
    m_displayItemList.clear();
    m_paintChunks.clear();
}

size_t PaintArtifact::approximateUnsharedMemoryUsage() const
{
    return sizeof(*this) + m_displayItemList.memoryUsageInBytes()
        + m_paintChunks.capacity() * sizeof(m_paintChunks[0]);
}

void PaintArtifact::replay(GraphicsContext& graphicsContext) const
{
    TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
    for (const DisplayItem& displayItem : m_displayItemList)
        displayItem.replay(graphicsContext);
}

void PaintArtifact::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    TRACE_EVENT0("blink,benchmark", "PaintArtifact::appendToWebDisplayItemList");
#if ENABLE(ASSERT)
    m_displayItemList.assertDisplayItemClientsAreAlive();
#endif
    for (const DisplayItem& displayItem : m_displayItemList) {
        // TODO(wkorman): Pass the actual visual rect for the display item.
        displayItem.appendToWebDisplayItemList(IntRect(), list);
    }
}

} // namespace blink

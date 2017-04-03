// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintArtifact.h"

#include "cc/paint/display_item_list.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/compositing/PaintChunksToCcLayer.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace blink {

namespace {

void computeChunkBoundsAndOpaqueness(const DisplayItemList& displayItems,
                                     Vector<PaintChunk>& paintChunks) {
  for (PaintChunk& chunk : paintChunks) {
    FloatRect bounds;
    SkRegion knownToBeOpaqueRegion;
    for (const DisplayItem& item : displayItems.itemsInPaintChunk(chunk)) {
      bounds.unite(FloatRect(item.client().visualRect()));
      if (!item.isDrawing())
        continue;
      const auto& drawing = static_cast<const DrawingDisplayItem&>(item);
      if (const PaintRecord* record = drawing.GetPaintRecord().get()) {
        if (drawing.knownToBeOpaque()) {
          // TODO(pdr): It may be too conservative to round in to the
          // enclosedIntRect.
          SkIRect conservativeRoundedRect;
          const SkRect& cullRect = record->cullRect();
          cullRect.roundIn(&conservativeRoundedRect);
          knownToBeOpaqueRegion.op(conservativeRoundedRect,
                                   SkRegion::kUnion_Op);
        }
      }
    }
    chunk.bounds = bounds;
    if (knownToBeOpaqueRegion.contains(enclosingIntRect(bounds)))
      chunk.knownToBeOpaque = true;
  }
}

}  // namespace

PaintArtifact::PaintArtifact()
    : m_displayItemList(0), m_isSuitableForGpuRasterization(true) {}

PaintArtifact::PaintArtifact(DisplayItemList displayItems,
                             Vector<PaintChunk> paintChunks,
                             bool isSuitableForGpuRasterizationArg)
    : m_displayItemList(std::move(displayItems)),
      m_paintChunks(std::move(paintChunks)),
      m_isSuitableForGpuRasterization(isSuitableForGpuRasterizationArg) {
  computeChunkBoundsAndOpaqueness(m_displayItemList, m_paintChunks);
}

PaintArtifact::PaintArtifact(PaintArtifact&& source)
    : m_displayItemList(std::move(source.m_displayItemList)),
      m_paintChunks(std::move(source.m_paintChunks)),
      m_isSuitableForGpuRasterization(source.m_isSuitableForGpuRasterization) {}

PaintArtifact::~PaintArtifact() {}

PaintArtifact& PaintArtifact::operator=(PaintArtifact&& source) {
  m_displayItemList = std::move(source.m_displayItemList);
  m_paintChunks = std::move(source.m_paintChunks);
  m_isSuitableForGpuRasterization = source.m_isSuitableForGpuRasterization;
  return *this;
}

void PaintArtifact::reset() {
  m_displayItemList.clear();
  m_paintChunks.clear();
}

size_t PaintArtifact::approximateUnsharedMemoryUsage() const {
  return sizeof(*this) + m_displayItemList.memoryUsageInBytes() +
         m_paintChunks.capacity() * sizeof(m_paintChunks[0]);
}

void PaintArtifact::replay(const FloatRect& bounds,
                           GraphicsContext& graphicsContext) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    for (const DisplayItem& displayItem : m_displayItemList)
      displayItem.replay(graphicsContext);
  } else {
    replay(bounds, *graphicsContext.canvas());
  }
}

void PaintArtifact::replay(const FloatRect& bounds,
                           PaintCanvas& canvas,
                           const PropertyTreeState& replayState) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
  Vector<const PaintChunk*> pointerPaintChunks;
  pointerPaintChunks.reserveInitialCapacity(paintChunks().size());

  // TODO(chrishtr): it's sad to have to copy this vector just to turn
  // references into pointers.
  for (const auto& chunk : paintChunks())
    pointerPaintChunks.push_back(&chunk);
  scoped_refptr<cc::DisplayItemList> displayItemList =
      PaintChunksToCcLayer::convert(pointerPaintChunks, replayState,
                                    gfx::Vector2dF(), getDisplayItemList());
  canvas.drawDisplayItemList(displayItemList);
}

DISABLE_CFI_PERF
void PaintArtifact::appendToWebDisplayItemList(WebDisplayItemList* list) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::appendToWebDisplayItemList");
  size_t visualRectIndex = 0;
  for (const DisplayItem& displayItem : m_displayItemList) {
    displayItem.appendToWebDisplayItemList(
        m_displayItemList.visualRect(visualRectIndex), list);
    visualRectIndex++;
  }
  list->setIsSuitableForGpuRasterization(isSuitableForGpuRasterization());
}

}  // namespace blink

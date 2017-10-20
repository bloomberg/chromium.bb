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

void ComputeChunkBoundsAndOpaqueness(const DisplayItemList& display_items,
                                     Vector<PaintChunk>& paint_chunks) {
  for (PaintChunk& chunk : paint_chunks) {
    FloatRect bounds;
    SkRegion known_to_be_opaque_region;
    for (const DisplayItem& item : display_items.ItemsInPaintChunk(chunk)) {
      bounds.Unite(FloatRect(item.Client().VisualRect()));
      if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          !item.IsDrawing())
        continue;
      const auto& drawing = static_cast<const DrawingDisplayItem&>(item);
      if (drawing.GetPaintRecord() && drawing.KnownToBeOpaque()) {
        // TODO(pdr): It may be too conservative to round in to the
        // EnclosedIntRect.
        SkIRect conservative_rounded_rect;
        const SkRect& record_bounds = drawing.GetPaintRecordBounds();
        record_bounds.roundIn(&conservative_rounded_rect);
        known_to_be_opaque_region.op(conservative_rounded_rect,
                                     SkRegion::kUnion_Op);
      }
    }
    chunk.bounds = bounds;
    if (known_to_be_opaque_region.contains(EnclosingIntRect(bounds)))
      chunk.known_to_be_opaque = true;
  }
}

}  // namespace

PaintArtifact::PaintArtifact() : display_item_list_(0) {}

PaintArtifact::PaintArtifact(DisplayItemList display_items,
                             Vector<PaintChunk> paint_chunks)
    : display_item_list_(std::move(display_items)),
      paint_chunks_(std::move(paint_chunks)) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    ComputeChunkBoundsAndOpaqueness(display_item_list_, paint_chunks_);
}

PaintArtifact::PaintArtifact(PaintArtifact&& source)
    : display_item_list_(std::move(source.display_item_list_)),
      paint_chunks_(std::move(source.paint_chunks_)) {}

PaintArtifact::~PaintArtifact() {}

PaintArtifact& PaintArtifact::operator=(PaintArtifact&& source) {
  display_item_list_ = std::move(source.display_item_list_);
  paint_chunks_ = std::move(source.paint_chunks_);
  return *this;
}

void PaintArtifact::Reset() {
  display_item_list_.Clear();
  paint_chunks_.clear();
}

size_t PaintArtifact::ApproximateUnsharedMemoryUsage() const {
  return sizeof(*this) + display_item_list_.MemoryUsageInBytes() +
         paint_chunks_.capacity() * sizeof(paint_chunks_[0]);
}

void PaintArtifact::Replay(GraphicsContext& graphics_context) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    for (const DisplayItem& display_item : display_item_list_)
      display_item.Replay(graphics_context);
  } else {
    Replay(*graphics_context.Canvas());
  }
}

void PaintArtifact::Replay(PaintCanvas& canvas,
                           const PropertyTreeState& replay_state) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());
  Vector<const PaintChunk*> pointer_paint_chunks;
  pointer_paint_chunks.ReserveInitialCapacity(PaintChunks().size());

  // TODO(chrishtr): it's sad to have to copy this vector just to turn
  // references into pointers.
  for (const auto& chunk : PaintChunks())
    pointer_paint_chunks.push_back(&chunk);
  scoped_refptr<cc::DisplayItemList> display_item_list =
      PaintChunksToCcLayer::Convert(
          pointer_paint_chunks, replay_state, gfx::Vector2dF(),
          GetDisplayItemList(),
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer);
  canvas.drawPicture(display_item_list->ReleaseAsRecord());
}

DISABLE_CFI_PERF
void PaintArtifact::AppendToWebDisplayItemList(
    const LayoutSize& visual_rect_offset,
    WebDisplayItemList* list) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::appendToWebDisplayItemList");
  for (const DisplayItem& item : display_item_list_)
    item.AppendToWebDisplayItemList(visual_rect_offset, list);
}

}  // namespace blink

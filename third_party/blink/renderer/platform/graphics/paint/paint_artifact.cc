// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/hit_test_display_item.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace blink {

namespace {

void ComputeChunkDerivedData(const DisplayItemList& display_items,
                             PaintChunk& chunk) {
  // This happens in tests testing paint chunks without display items.
  if (!chunk.size())
    return;

  SkRegion known_to_be_opaque_region;
  for (const DisplayItem& item : display_items.ItemsInPaintChunk(chunk)) {
    chunk.bounds.Unite(item.VisualRect());
    chunk.outset_for_raster_effects = std::max(chunk.outset_for_raster_effects,
                                               item.OutsetForRasterEffects());

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        item.IsDrawing()) {
      const auto& drawing = static_cast<const DrawingDisplayItem&>(item);
      if (drawing.GetPaintRecord() && drawing.KnownToBeOpaque()) {
        known_to_be_opaque_region.op(
            SkIRect(EnclosedIntRect(drawing.VisualRect())),
            SkRegion::kUnion_Op);
      }
    }

    if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled() &&
        item.IsHitTest()) {
      const auto& hit_test = static_cast<const HitTestDisplayItem&>(item);
      if (!chunk.hit_test_data)
        chunk.hit_test_data = std::make_unique<HitTestData>();
      chunk.hit_test_data->Append(hit_test.GetHitTestRect());
    }
  }

  if (known_to_be_opaque_region.contains(EnclosingIntRect(chunk.bounds)))
    chunk.known_to_be_opaque = true;
}

// For PaintArtifact::AppendDebugDrawing().
class DebugDrawingClient final : public DisplayItemClient {
 public:
  DebugDrawingClient() { Invalidate(PaintInvalidationReason::kUncacheable); }
  String DebugName() const final { return "DebugDrawing"; }
  LayoutRect VisualRect() const final {
    return LayoutRect(LayoutRect::InfiniteIntRect());
  }
};

}  // namespace

PaintArtifact::PaintArtifact() : display_item_list_(0) {}

PaintArtifact::PaintArtifact(DisplayItemList display_items,
                             Vector<PaintChunk> chunks)
    : display_item_list_(std::move(display_items)), chunks_(std::move(chunks)) {
  for (auto& chunk : chunks_)
    ComputeChunkDerivedData(display_item_list_, chunk);
}

PaintArtifact::~PaintArtifact() = default;

scoped_refptr<PaintArtifact> PaintArtifact::Create(
    DisplayItemList display_items,
    Vector<PaintChunk> chunks) {
  return base::AdoptRef(
      new PaintArtifact(std::move(display_items), std::move(chunks)));
}

scoped_refptr<PaintArtifact> PaintArtifact::Empty() {
  DEFINE_STATIC_REF(PaintArtifact, empty, base::AdoptRef(new PaintArtifact()));
  return empty;
}

size_t PaintArtifact::ApproximateUnsharedMemoryUsage() const {
  size_t total_size = sizeof(*this) + display_item_list_.MemoryUsageInBytes() +
                      chunks_.capacity() * sizeof(chunks_[0]);
  for (const auto& chunk : chunks_)
    total_size += chunk.MemoryUsageInBytes();
  return total_size;
}

void PaintArtifact::AppendDebugDrawing(
    sk_sp<const PaintRecord> record,
    const PropertyTreeState& property_tree_state) {
  DEFINE_STATIC_LOCAL(DebugDrawingClient, debug_drawing_client, ());

  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  auto& display_item =
      display_item_list_.AllocateAndConstruct<DrawingDisplayItem>(
          debug_drawing_client, DisplayItem::kDebugDrawing, std::move(record));

  // Create a PaintChunk for the debug drawing.
  chunks_.emplace_back(display_item_list_.size() - 1, display_item_list_.size(),
                       display_item.GetId(), property_tree_state);
  ComputeChunkDerivedData(display_item_list_, chunks_.back());
}

void PaintArtifact::Replay(GraphicsContext& graphics_context,
                           const PropertyTreeState& replay_state,
                           const IntPoint& offset) const {
  Replay(*graphics_context.Canvas(), replay_state, offset);
}

void PaintArtifact::Replay(cc::PaintCanvas& canvas,
                           const PropertyTreeState& replay_state,
                           const IntPoint& offset) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  scoped_refptr<cc::DisplayItemList> display_item_list =
      PaintChunksToCcLayer::Convert(
          PaintChunks(), replay_state, gfx::Vector2dF(offset.X(), offset.Y()),
          GetDisplayItemList(),
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer);
  canvas.drawPicture(display_item_list->ReleaseAsRecord());
}

DISABLE_CFI_PERF
void PaintArtifact::AppendToDisplayItemList(const FloatSize& visual_rect_offset,
                                            cc::DisplayItemList& list) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::AppendToDisplayItemList");
  for (const DisplayItem& item : display_item_list_)
    item.AppendToDisplayItemList(visual_rect_offset, list);
}

void PaintArtifact::FinishCycle() {
  // BlinkGenPropertyTrees uses PaintController::ClearPropertyTreeChangedStateTo
  // for clearing the property tree changed state at the end of paint instead of
  // in FinishCycle. See: LocalFrameView::RunPaintLifecyclePhase.
  bool clear_property_tree_changed =
      !RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled();
  for (auto& chunk : chunks_) {
    chunk.client_is_just_created = false;
    if (clear_property_tree_changed)
      chunk.properties.ClearChangedToRoot();
  }
}

}  // namespace blink

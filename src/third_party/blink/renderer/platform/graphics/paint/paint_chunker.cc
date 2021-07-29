// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "cc/base/features.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

namespace blink {

void PaintChunker::ResetChunks(Vector<PaintChunk>* chunks) {
  if (chunks_) {
    FinalizeLastChunkProperties();
    SetWillForceNewChunk(true);
    current_properties_ = PropertyTreeState::Uninitialized();
  }
  chunks_ = chunks;
#if DCHECK_IS_ON()
  DCHECK(!chunks || chunks->IsEmpty());
  DCHECK(IsInInitialState());
#endif
}

#if DCHECK_IS_ON()
bool PaintChunker::IsInInitialState() const {
  if (current_properties_ != PropertyTreeState::Uninitialized())
    return false;
  DCHECK_EQ(candidate_background_color_.Rgb(), Color::kTransparent);
  DCHECK_EQ(candidate_background_area_, 0u);
  DCHECK(will_force_new_chunk_);
  DCHECK(!chunks_ || chunks_->IsEmpty());
  return true;
}
#endif

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id* chunk_id,
    const PropertyTreeStateOrAlias& properties) {
  // If properties are the same, continue to use the previously set
  // |next_chunk_id_| because the id of the outer painting is likely to be
  // more stable to reduce invalidation because of chunk id changes.
  if (!next_chunk_id_ || current_properties_ != properties) {
    if (chunk_id)
      next_chunk_id_.emplace(*chunk_id);
    else
      next_chunk_id_ = absl::nullopt;
  }
  current_properties_ = properties;
}

void PaintChunker::AppendByMoving(PaintChunk&& chunk) {
  DCHECK(chunks_);
  FinalizeLastChunkProperties();
  wtf_size_t next_chunk_begin_index =
      chunks_->IsEmpty() ? 0 : chunks_->back().end_index;
  chunks_->emplace_back(next_chunk_begin_index, std::move(chunk));
}

bool PaintChunker::EnsureCurrentChunk(const PaintChunk::Id& id) {
#if DCHECK_IS_ON()
  DCHECK(chunks_);
  // If this DCHECKs are hit we are missing a call to update the properties.
  // See: ScopedPaintChunkProperties.
  DCHECK(!IsInInitialState());
  // At this point we should have all of the properties given to us.
  DCHECK(current_properties_.IsInitialized());
#endif

  if (WillForceNewChunk() ||
      current_properties_ != chunks_->back().properties) {
    if (!next_chunk_id_)
      next_chunk_id_.emplace(id);
    FinalizeLastChunkProperties();
    wtf_size_t begin = chunks_->IsEmpty() ? 0 : chunks_->back().end_index;
    chunks_->emplace_back(begin, begin, *next_chunk_id_, current_properties_,
                          current_effectively_invisible_);
    next_chunk_id_ = absl::nullopt;
    will_force_new_chunk_ = false;
    return true;
  }
  return false;
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItem& item) {
  DCHECK(chunks_);

  bool item_forces_new_chunk = item.IsForeignLayer() || item.IsScrollbar();
  if (item_forces_new_chunk)
    SetWillForceNewChunk(true);

  bool created_new_chunk = EnsureCurrentChunk(item.GetId());
  auto& chunk = chunks_->back();

  chunk.bounds.Unite(item.VisualRect());
  if (item.DrawsContent())
    chunk.drawable_bounds.Unite(item.VisualRect());

  // If this paints the background and it's larger than our current candidate,
  // set the candidate to be this item.
  if (item.IsDrawing() && item.DrawsContent()) {
    float item_area;
    Color item_color = To<DrawingDisplayItem>(item).BackgroundColor(item_area);
    ProcessBackgroundColorCandidate(chunk.id, item_color, item_area);
  }

  if (should_compute_contents_opaque_ && item.IsDrawing()) {
    const DrawingDisplayItem& drawing = To<DrawingDisplayItem>(item);
    if (drawing.KnownToBeOpaque()) {
      chunk.rect_known_to_be_opaque =
          MaximumCoveredRect(chunk.rect_known_to_be_opaque, item.VisualRect());
    }
    if (chunk.text_known_to_be_on_opaque_background) {
      if (const auto* paint_record = drawing.GetPaintRecord().get()) {
        if (paint_record->has_draw_text_ops()) {
          chunk.has_text = true;
          chunk.text_known_to_be_on_opaque_background =
              chunk.rect_known_to_be_opaque.Contains(item.VisualRect());
        }
      }
    } else {
      // text_known_to_be_on_opaque_background should be initially true before
      // we see any text.
      DCHECK(chunk.has_text);
    }
  }

  chunk.raster_effect_outset =
      std::max(chunk.raster_effect_outset, item.GetRasterEffectOutset());

  chunk.end_index++;

  // When forcing a new chunk, we still need to force new chunk for the next
  // display item. Otherwise reset force_new_chunk_ to false.
  DCHECK(!will_force_new_chunk_);
  if (item_forces_new_chunk) {
    DCHECK(created_new_chunk);
    SetWillForceNewChunk(true);
  }

  return created_new_chunk;
}

bool PaintChunker::AddHitTestDataToCurrentChunk(const PaintChunk::Id& id,
                                                const IntRect& rect,
                                                TouchAction touch_action,
                                                bool blocking_wheel) {
  // In CompositeAfterPaint, we ensure a paint chunk for correct composited
  // hit testing. In pre-CompositeAfterPaint, this is unnecessary, except that
  // there is special touch action or blocking wheel event handler, and that we
  // have a non-root effect so that PaintChunksToCcLayer will emit paint
  // operations for filters.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      touch_action == TouchAction::kAuto && !blocking_wheel &&
      &current_properties_.Effect() == &EffectPaintPropertyNode::Root())
    return false;

  bool created_new_chunk = EnsureCurrentChunk(id);
  auto& chunk = chunks_->back();
  chunk.bounds.Unite(rect);
  if (touch_action != TouchAction::kAuto) {
    chunk.EnsureHitTestData().touch_action_rects.push_back(
        TouchActionRect{rect, touch_action});
  }
  if (blocking_wheel) {
    DCHECK(base::FeatureList::IsEnabled(::features::kWheelEventRegions));
    chunk.EnsureHitTestData().wheel_event_rects.push_back(rect);
  }
  return created_new_chunk;
}

void PaintChunker::AddSelectionToCurrentChunk(
    absl::optional<PaintedSelectionBound> start,
    absl::optional<PaintedSelectionBound> end) {
  // We should have painted the selection when calling this method.
  DCHECK(chunks_);
  DCHECK(!chunks_->IsEmpty());

  auto& chunk = chunks_->back();

#if DCHECK_IS_ON()
  if (start) {
    IntRect edge_rect(start->edge_start, start->edge_end - start->edge_start);
    DCHECK(chunk.bounds.Contains(edge_rect));
  }

  if (end) {
    IntRect edge_rect(end->edge_start, end->edge_end - end->edge_start);
    DCHECK(chunk.bounds.Contains(edge_rect));
  }
#endif

  LayerSelectionData& selection_data = chunk.EnsureLayerSelectionData();
  if (start) {
    DCHECK(!selection_data.start);
    selection_data.start = start;
  }

  if (end) {
    DCHECK(!selection_data.end);
    selection_data.end = end;
  }
}

void PaintChunker::CreateScrollHitTestChunk(
    const PaintChunk::Id& id,
    const TransformPaintPropertyNode* scroll_translation,
    const IntRect& rect) {
#if DCHECK_IS_ON()
  if (id.type == DisplayItem::Type::kResizerScrollHitTest ||
      id.type == DisplayItem::Type::kPluginScrollHitTest ||
      id.type == DisplayItem::Type::kCustomScrollbarHitTest) {
    // Resizer and plugin scroll hit tests are only used to prevent composited
    // scrolling and should not have a scroll offset node.
    DCHECK(!scroll_translation);
  } else if (id.type == DisplayItem::Type::kScrollHitTest) {
    DCHECK(scroll_translation);
    // The scroll offset transform node should have an associated scroll node.
    DCHECK(scroll_translation->ScrollNode());
  } else {
    NOTREACHED();
  }
#endif

  SetWillForceNewChunk(true);
  bool created_new_chunk = EnsureCurrentChunk(id);
  DCHECK(created_new_chunk);

  auto& chunk = chunks_->back();
  chunk.bounds.Unite(rect);
  auto& hit_test_data = chunk.EnsureHitTestData();
  hit_test_data.scroll_translation = scroll_translation;
  hit_test_data.scroll_hit_test_rect = rect;
  SetWillForceNewChunk(true);
}

bool PaintChunker::ProcessBackgroundColorCandidate(const PaintChunk::Id& id,
                                                   Color color,
                                                   float area) {
  if (color == Color::kTransparent)
    return false;

  bool created_new_chunk = EnsureCurrentChunk(id);
  float min_background_area = kMinBackgroundColorCoverageRatio *
                              chunks_->back().bounds.Width() *
                              chunks_->back().bounds.Height();
  if (created_new_chunk || area >= candidate_background_area_ ||
      area >= min_background_area) {
    candidate_background_color_ =
        candidate_background_area_ >= min_background_area
            ? candidate_background_color_.Blend(color)
            : color;
    candidate_background_area_ = area;
  }
  return created_new_chunk;
}

void PaintChunker::FinalizeLastChunkProperties() {
  DCHECK(chunks_);
  if (chunks_->IsEmpty() || chunks_->back().is_moved_from_cached_subsequence)
    return;

  auto& chunk = chunks_->back();
  if (candidate_background_color_ != Color::kTransparent) {
    chunk.background_color = candidate_background_color_;
    chunk.background_color_area = candidate_background_area_;
  }
  candidate_background_color_ = Color::kTransparent;
  candidate_background_area_ = 0u;
}

}  // namespace blink

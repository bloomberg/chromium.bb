// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PaintChunker::PaintChunker() : force_new_chunk_(false) {}

PaintChunker::~PaintChunker() = default;

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const Optional<PaintChunk::Id>& chunk_id,
    const PaintChunkProperties& properties) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());

  if (chunk_id)
    current_chunk_id_.emplace(*chunk_id);
  else
    current_chunk_id_ = WTF::nullopt;
  current_properties_ = properties;
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItem& item) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());

#if DCHECK_IS_ON()
  // Property nodes should never be null because they should either be set to
  // properties created by a LayoutObject/FrameView, or be set to a non-null
  // root node. If these DCHECKs are hit we are missing a call to update the
  // properties. See: ScopedPaintChunkProperties.
  // TODO(trchen): Enable this check for SPv175 too. Some drawable layers
  // don't paint with property tree yet, e.g. scrollbar layers.
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    DCHECK(current_properties_.property_tree_state.Transform());
    DCHECK(current_properties_.property_tree_state.Clip());
    DCHECK(current_properties_.property_tree_state.Effect());
  }
#endif

  bool item_forces_new_chunk = item.IsForeignLayer() || item.IsScrollHitTest();
  if (item_forces_new_chunk) {
    force_new_chunk_ = true;
    // Clear current_chunk_id_ so that we will use the current display item's id
    // as the chunk id, and any display items after the forcing item without a
    // new chunk id will be treated as having no id to avoid the chunk from
    // using the same id as the chunk before the forced chunk.
    current_chunk_id_ = WTF::nullopt;
  } else if (force_new_chunk_ && data_.chunks.size() &&
             data_.chunks.back().id == current_chunk_id_) {
    // For other forced_new_chunk_ reasons (e.g. subsequences), use the first
    // display items' id if the client didn't specify an id for the forced new
    // chunk (i.e. |current_chunk_id_| has been used by the previous chunk).
    current_chunk_id_ = WTF::nullopt;
  }

  size_t new_chunk_begin_index;
  if (data_.chunks.IsEmpty()) {
    new_chunk_begin_index = 0;
  } else {
    auto& last_chunk = LastChunk();
    if (!force_new_chunk_ && current_properties_ == last_chunk.properties &&
        (!current_chunk_id_ || current_chunk_id_ == last_chunk.id)) {
      // Continue the current chunk.
      last_chunk.end_index++;
      return false;
    }
    new_chunk_begin_index = last_chunk.end_index;
  }

  auto cacheable =
      item.SkippedCache() ? PaintChunk::kUncacheable : PaintChunk::kCacheable;
  PaintChunk new_chunk(new_chunk_begin_index, new_chunk_begin_index + 1,
                       current_chunk_id_ ? *current_chunk_id_ : item.GetId(),
                       current_properties_, cacheable);
  data_.chunks.push_back(new_chunk);

  // When forcing a new chunk, we still need to force new chunk for the next
  // display item. Otherwise reset force_new_chunk_ to false.
  if (!item_forces_new_chunk)
    force_new_chunk_ = false;

  return true;
}

void PaintChunker::TrackRasterInvalidation(const PaintChunk& chunk,
                                           const RasterInvalidationInfo& info) {
  size_t index = ChunkIndex(chunk);
  auto& trackings = data_.raster_invalidation_trackings;
  if (trackings.size() <= index)
    trackings.resize(index + 1);
  trackings[index].push_back(info);
}

void PaintChunker::Clear() {
  data_.Clear();
  current_chunk_id_ = WTF::nullopt;
  current_properties_ = PaintChunkProperties();
}

PaintChunksAndRasterInvalidations PaintChunker::ReleaseData() {
  current_chunk_id_ = WTF::nullopt;
  current_properties_ = PaintChunkProperties();
  data_.chunks.ShrinkToFit();
  data_.raster_invalidation_rects.ShrinkToFit();
  return std::move(data_);
}

}  // namespace blink

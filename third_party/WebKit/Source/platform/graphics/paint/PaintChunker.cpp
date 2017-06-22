// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

PaintChunker::PaintChunker() : force_new_chunk_(kDontForceNewChunk) {}

PaintChunker::~PaintChunker() {}

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id* chunk_id,
    const PaintChunkProperties& properties,
    NewChunkForceState force_new_chunk) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  current_chunk_id_ = WTF::nullopt;
  if (chunk_id)
    current_chunk_id_.emplace(*chunk_id);
  current_properties_ = properties;
  force_new_chunk_ = force_new_chunk;
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItem& item) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

#if DCHECK_IS_ON()
  // Property nodes should never be null because they should either be set to
  // properties created by a LayoutObject/FrameView, or be set to a non-null
  // root node. If these DCHECKs are hit we are missing a call to update the
  // properties. See: ScopedPaintChunkProperties.
  DCHECK(current_properties_.property_tree_state.Transform());
  DCHECK(current_properties_.property_tree_state.Clip());
  DCHECK(current_properties_.property_tree_state.Effect());
#endif

  ItemBehavior behavior;
  if (DisplayItem::IsForeignLayerType(item.GetType())) {
    behavior = kRequiresSeparateChunk;
    // Clear current_chunk_id_ so that we will use the current display item's id
    // as the chunk id, and any display items after the foreign layer without a
    // new chunk id will be treated as having no id to avoid the chunk from
    // using the same id as the chunk before the foreign layer chunk.
    current_chunk_id_ = WTF::nullopt;
  } else {
    behavior = kDefaultBehavior;
  }

  size_t new_chunk_begin_index;
  if (chunks_.IsEmpty()) {
    new_chunk_begin_index = 0;
  } else {
    auto& last_chunk = chunks_.back();
    bool can_continue_chunk =
        current_properties_ == last_chunk.properties &&
        behavior != kRequiresSeparateChunk &&
        chunk_behavior_.back() != kRequiresSeparateChunk &&
        force_new_chunk_ == kDontForceNewChunk;
    if (can_continue_chunk) {
      last_chunk.end_index++;
      return false;
    }
    new_chunk_begin_index = last_chunk.end_index;
  }

  force_new_chunk_ = kDontForceNewChunk;

  auto cacheable =
      item.SkippedCache() ? PaintChunk::kUncacheable : PaintChunk::kCacheable;
  PaintChunk new_chunk(new_chunk_begin_index, new_chunk_begin_index + 1,
                       current_chunk_id_ ? *current_chunk_id_ : item.GetId(),
                       current_properties_, cacheable);
  chunks_.push_back(new_chunk);
  chunk_behavior_.push_back(behavior);
  return true;
}

void PaintChunker::DecrementDisplayItemIndex() {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  DCHECK(!chunks_.IsEmpty());

  auto& last_chunk = chunks_.back();
  if ((last_chunk.end_index - last_chunk.begin_index) > 1) {
    last_chunk.end_index--;
    return;
  }

  chunks_.pop_back();
  chunk_behavior_.pop_back();
}

void PaintChunker::Clear() {
  chunks_.clear();
  chunk_behavior_.clear();
  current_chunk_id_ = WTF::nullopt;
  current_properties_ = PaintChunkProperties();
}

Vector<PaintChunk> PaintChunker::ReleasePaintChunks() {
  Vector<PaintChunk> chunks;
  chunks.swap(chunks_);
  chunk_behavior_.clear();
  current_chunk_id_ = WTF::nullopt;
  current_properties_ = PaintChunkProperties();
  return chunks;
}

}  // namespace blink

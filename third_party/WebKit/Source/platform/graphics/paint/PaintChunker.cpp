// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

PaintChunker::PaintChunker() : force_new_chunk_(DontForceNewChunk) {}

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
  Optional<PaintChunk::Id> new_chunk_id;
  PaintChunk::Cacheable cacheable =
      item.SkippedCache() ? PaintChunk::kUncacheable : PaintChunk::kCacheable;
  if (DisplayItem::IsForeignLayerType(item.GetType())) {
    behavior = kRequiresSeparateChunk;
    // Use null chunkId if we are skipping cache, so that the chunk will not
    // match any old chunk and will be treated as brand new.
    new_chunk_id.emplace(item.GetId());

    // Clear m_currentChunkId so that any display items after the foreign layer
    // without a new chunk id will be treated as having no id to avoid the chunk
    // from using the same id as the chunk before the foreign layer chunk.
    current_chunk_id_ = WTF::nullopt;
  } else {
    behavior = kDefaultBehavior;
    if (current_chunk_id_) {
      new_chunk_id.emplace(*current_chunk_id_);
    } else {
      cacheable = PaintChunk::kUncacheable;
      new_chunk_id.emplace(item.GetId());
    }
  }

  if (chunks_.IsEmpty()) {
    PaintChunk new_chunk(0, 1, *new_chunk_id, current_properties_, cacheable);
    chunks_.push_back(new_chunk);
    chunk_behavior_.push_back(behavior);
    force_new_chunk_ = DontForceNewChunk;
    return true;
  }

  auto& last_chunk = chunks_.back();
  bool can_continue_chunk = current_properties_ == last_chunk.properties &&
                            behavior != kRequiresSeparateChunk &&
                            chunk_behavior_.back() != kRequiresSeparateChunk &&
                            force_new_chunk_ == DontForceNewChunk;
  if (can_continue_chunk) {
    last_chunk.end_index++;
    return false;
  }

  force_new_chunk_ = DontForceNewChunk;

  PaintChunk new_chunk(last_chunk.end_index, last_chunk.end_index + 1,
                       *new_chunk_id, current_properties_, cacheable);
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

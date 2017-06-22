// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunker_h
#define PaintChunker_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

enum NewChunkForceState { kDontForceNewChunk, kForceNewChunk };

// Accepts information about changes to |PaintChunkProperties| as drawings are
// accumulated, and produces a series of paint chunks: contiguous ranges of the
// display list with identical |PaintChunkProperties|.
class PLATFORM_EXPORT PaintChunker final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(PaintChunker);

 public:
  PaintChunker();
  ~PaintChunker();

  bool IsInInitialState() const {
    return chunks_.IsEmpty() && current_properties_ == PaintChunkProperties();
  }

  const PaintChunkProperties& CurrentPaintChunkProperties() const {
    return current_properties_;
  }
  void UpdateCurrentPaintChunkProperties(
      const PaintChunk::Id*,
      const PaintChunkProperties&,
      NewChunkForceState force_new_chunk = kDontForceNewChunk);

  // Returns true if a new chunk is created.
  bool IncrementDisplayItemIndex(const DisplayItem&);

  void DecrementDisplayItemIndex();

  PaintChunk& PaintChunkAt(size_t i) { return chunks_[i]; }
  size_t LastChunkIndex() const {
    return chunks_.IsEmpty() ? kNotFound : chunks_.size() - 1;
  }
  PaintChunk& LastChunk() { return chunks_.back(); }

  PaintChunk& FindChunkByDisplayItemIndex(size_t index) {
    auto chunk = FindChunkInVectorByDisplayItemIndex(chunks_, index);
    DCHECK(chunk != chunks_.end());
    return *chunk;
  }

  void Clear();

  // Releases the generated paint chunk list and resets the state of this
  // object.
  Vector<PaintChunk> ReleasePaintChunks();

 private:
  enum ItemBehavior {
    // Can be combined with adjacent items when building chunks.
    kDefaultBehavior = 0,

    // Item requires its own paint chunk.
    kRequiresSeparateChunk,
  };

  Vector<PaintChunk> chunks_;
  Vector<ItemBehavior> chunk_behavior_;
  Optional<PaintChunk::Id> current_chunk_id_;
  PaintChunkProperties current_properties_;
  NewChunkForceState force_new_chunk_;
};

}  // namespace blink

#endif  // PaintChunker_h

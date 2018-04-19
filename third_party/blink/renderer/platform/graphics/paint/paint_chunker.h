// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/optional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

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
    return data_.chunks.IsEmpty() &&
           current_properties_ == PaintChunkProperties();
  }

  const PaintChunkProperties& CurrentPaintChunkProperties() const {
    return current_properties_;
  }
  void UpdateCurrentPaintChunkProperties(const Optional<PaintChunk::Id>&,
                                         const PaintChunkProperties&);

  void ForceNewChunk() { force_new_chunk_ = true; }

  // Returns true if a new chunk is created.
  bool IncrementDisplayItemIndex(const DisplayItem&);

  const Vector<PaintChunk>& PaintChunks() const { return data_.chunks; }

  PaintChunk& PaintChunkAt(size_t i) { return data_.chunks[i]; }
  size_t LastChunkIndex() const {
    return data_.chunks.IsEmpty() ? kNotFound : data_.chunks.size() - 1;
  }
  PaintChunk& LastChunk() { return data_.chunks.back(); }

  PaintChunk& FindChunkByDisplayItemIndex(size_t index) {
    auto chunk = FindChunkInVectorByDisplayItemIndex(data_.chunks, index);
    DCHECK(chunk != data_.chunks.end());
    return *chunk;
  }

  void AddRasterInvalidation(const PaintChunk& chunk, const FloatRect& rect) {
    size_t index = ChunkIndex(chunk);
    auto& rects = data_.raster_invalidation_rects;
    if (rects.size() <= index)
      rects.resize(index + 1);
    rects[index].push_back(rect);
  }

  void TrackRasterInvalidation(const PaintChunk&,
                               const RasterInvalidationInfo&);

  void Clear();

  // Releases the generated paint chunk list and raster invalidations and
  // resets the state of this object.
  PaintChunksAndRasterInvalidations ReleaseData();

 private:
  size_t ChunkIndex(const PaintChunk& chunk) const {
    size_t index = &chunk - &data_.chunks.front();
    DCHECK_LT(index, data_.chunks.size());
    return index;
  }

  PaintChunksAndRasterInvalidations data_;

  // TODO(pdr): Refactor current_chunk_id_ so that it is always the equal to
  // the current chunk id. This is currently not true when there is a forced
  // chunk because the current_chunk_id_ is cleared for subsequent chunks, even
  // though those subsequent chunks will have valid chunk ids.
  Optional<PaintChunk::Id> current_chunk_id_;
  PaintChunkProperties current_properties_;
  // True when an item forces a new chunk (e.g., foreign display items), and for
  // the item following a forced chunk.
  bool force_new_chunk_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNKER_H_

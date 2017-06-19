// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunk_h
#define PaintChunk_h

#include <iosfwd>
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"

namespace blink {

// A contiguous sequence of drawings with common paint properties.
//
// This is expected to be owned by the paint artifact which also owns the
// related drawings.
//
// This is a Slimming Paint v2 class.
struct PaintChunk {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  using Id = DisplayItem::Id;

  enum Cacheable {
    kCacheable,
    kUncacheable,
  };

  PaintChunk(size_t begin,
             size_t end,
             const Id& id,
             const PaintChunkProperties& props,
             Cacheable cacheable = kCacheable)
      : begin_index(begin),
        end_index(end),
        id(id),
        properties(props),
        outset_for_raster_effects(0),
        known_to_be_opaque(false),
        is_cacheable(cacheable == kCacheable) {}

  size_t size() const {
    DCHECK_GE(end_index, begin_index);
    return end_index - begin_index;
  }

  // Check if a new PaintChunk (this) created in the latest paint matches an old
  // PaintChunk created in the previous paint.
  bool Matches(const PaintChunk& old) const {
    return old.is_cacheable && Matches(old.id);
  }

  bool Matches(const Id& other_id) const {
    if (!is_cacheable || id != other_id)
      return false;
#if DCHECK_IS_ON()
    DCHECK(id.client.IsAlive());
#endif
    // A chunk whose client is just created should not match any cached chunk,
    // even if it's id equals the old chunk's id (which may happen if this
    // chunk's client is just created at the same address of the old chunk's
    // deleted client).
    return !id.client.IsJustCreated();
  }

  // Index of the first drawing in this chunk.
  size_t begin_index;

  // Index of the first drawing not in this chunk, so that there are
  // |endIndex - beginIndex| drawings in the chunk.
  size_t end_index;

  // Identifier of this chunk. If it has a value, it should be unique. This is
  // used to match a new chunk to a cached old chunk to track changes of chunk
  // contents, so the id should be stable across document cycles. If the
  // contents of the chunk can't be cached (e.g. it's created when
  // PaintController is skipping the cache, normally because display items can't
  // be uniquely identified), |id| is nullopt so that the chunk won't match any
  // other chunk.
  Id id;

  // The paint properties which apply to this chunk.
  PaintChunkProperties properties;

  // The total bounds of this paint chunk's contents, in the coordinate space of
  // the containing transform node.
  FloatRect bounds;

  // Some raster effects can exceed |bounds| in the rasterization space. This
  // is the maximum DisplayItemClient::VisualRectOutsetForRasterEffects() of
  // all clients of items in this chunk.
  float outset_for_raster_effects;

  // True if the bounds are filled entirely with opaque contents.
  bool known_to_be_opaque : 1;

  bool is_cacheable : 1;

  // SPv2 only. Rectangles that need to be re-rasterized in this chunk, in the
  // coordinate space of the containing transform node.
  Vector<FloatRect> raster_invalidation_rects;

  Vector<RasterInvalidationInfo> raster_invalidation_tracking;
};

inline bool operator==(const PaintChunk& a, const PaintChunk& b) {
  return a.begin_index == b.begin_index && a.end_index == b.end_index &&
         a.id == b.id && a.properties == b.properties && a.bounds == b.bounds &&
         a.known_to_be_opaque == b.known_to_be_opaque &&
         a.is_cacheable == b.is_cacheable &&
         a.raster_invalidation_rects == b.raster_invalidation_rects;
}

inline bool operator!=(const PaintChunk& a, const PaintChunk& b) {
  return !(a == b);
}

inline bool ChunkLessThanIndex(const PaintChunk& chunk, size_t index) {
  return chunk.end_index <= index;
}

inline Vector<PaintChunk>::iterator FindChunkInVectorByDisplayItemIndex(
    Vector<PaintChunk>& chunks,
    size_t index) {
  auto chunk =
      std::lower_bound(chunks.begin(), chunks.end(), index, ChunkLessThanIndex);
  DCHECK(chunk == chunks.end() ||
         (index >= chunk->begin_index && index < chunk->end_index));
  return chunk;
}

inline Vector<PaintChunk>::const_iterator FindChunkInVectorByDisplayItemIndex(
    const Vector<PaintChunk>& chunks,
    size_t index) {
  return FindChunkInVectorByDisplayItemIndex(
      const_cast<Vector<PaintChunk>&>(chunks), index);
}

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const PaintChunk&, std::ostream*);

}  // namespace blink

#endif  // PaintChunk_h

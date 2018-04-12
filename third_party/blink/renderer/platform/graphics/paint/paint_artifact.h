// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class GraphicsContext;
class WebDisplayItemList;

// The output of painting, consisting of a series of drawings in paint order,
// partitioned into discontiguous chunks with a common set of paint properties
// (i.e. associated with the same transform, clip, effects, etc.).
//
// It represents a particular state of the world, and should be immutable
// (const) to most of its users.
//
// Unless its dangerous accessors are used, it promises to be in a reasonable
// state (e.g. chunk bounding boxes computed).
//
// Reminder: moved-from objects may not be in a known state. They can only
// safely be assigned to or destroyed.
class PLATFORM_EXPORT PaintArtifact final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(PaintArtifact);

 public:
  PaintArtifact();
  PaintArtifact(DisplayItemList, Vector<PaintChunk>);
  PaintArtifact(PaintArtifact&&);
  ~PaintArtifact();

  PaintArtifact& operator=(PaintArtifact&&);

  bool IsEmpty() const { return display_item_list_.IsEmpty(); }

  DisplayItemList& GetDisplayItemList() { return display_item_list_; }
  const DisplayItemList& GetDisplayItemList() const {
    return display_item_list_;
  }

  Vector<PaintChunk>& PaintChunks() { return paint_chunks_; }
  const Vector<PaintChunk>& PaintChunks() const { return paint_chunks_; }

  PaintChunkSubset GetPaintChunkSubset(
      const Vector<size_t>& subset_indices) const {
    return PaintChunkSubset(paint_chunks_, subset_indices);
  }

  Vector<PaintChunk>::const_iterator FindChunkByDisplayItemIndex(
      size_t index) const {
    return FindChunkInVectorByDisplayItemIndex(paint_chunks_, index);
  }

  // Resets to an empty paint artifact.
  void Reset();

  // Returns the approximate memory usage, excluding memory likely to be
  // shared with the embedder after copying to WebDisplayItemList.
  size_t ApproximateUnsharedMemoryUsage() const;

  // Draws the paint artifact to a GraphicsContext.
  // In SPv175+ mode, replays into the ancestor state given by |replay_state|.
  void Replay(GraphicsContext&,
              const PropertyTreeState& replay_state,
              const IntPoint& offset = IntPoint()) const;

  // Draws the paint artifact to a PaintCanvas, into the ancestor state given
  // by |replay_state|. For SPv175+ only.
  void Replay(PaintCanvas&,
              const PropertyTreeState& replay_state,
              const IntPoint& offset = IntPoint()) const;

  // Writes the paint artifact into a WebDisplayItemList.
  void AppendToWebDisplayItemList(const FloatSize& visual_rect_offset,
                                  WebDisplayItemList*) const;

 private:
  DisplayItemList display_item_list_;
  Vector<PaintChunk> paint_chunks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_

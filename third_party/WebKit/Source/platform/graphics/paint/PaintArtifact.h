// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintArtifact_h
#define PaintArtifact_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

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
  void Replay(GraphicsContext&) const;

  // Draws the paint artifact to a PaintCanvas. For SPv175+ only.
  // Replays into the ancestor state given by |replay_state|.
  void Replay(
      PaintCanvas&,
      const PropertyTreeState& replay_state = PropertyTreeState::Root()) const;

  // Writes the paint artifact into a WebDisplayItemList.
  void AppendToWebDisplayItemList(const LayoutSize& visual_rect_offset,
                                  WebDisplayItemList*) const;

 private:
  DisplayItemList display_item_list_;
  Vector<PaintChunk> paint_chunks_;
};

}  // namespace blink

#endif  // PaintArtifact_h

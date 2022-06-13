// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// A PaintArtifact represents the output of painting, consisting of paint chunks
// and display items (in DisplayItemList).
//
// A PaintArtifact not only represents the output of the current painting, but
// also serves as cache of individual display items and paint chunks for later
// paintings as long as the display items and paint chunks are valid.
//
// It represents a particular state of the world, and is immutable (const) and
// promises to be in a reasonable state (e.g. chunk bounding boxes computed) to
// all users, except for PaintController and unit tests.
class PLATFORM_EXPORT PaintArtifact final : public RefCounted<PaintArtifact> {
  USING_FAST_MALLOC(PaintArtifact);

 public:
  PaintArtifact() = default;
  PaintArtifact(const PaintArtifact& other) = delete;
  PaintArtifact& operator=(const PaintArtifact& other) = delete;
  PaintArtifact(PaintArtifact&& other) = delete;
  PaintArtifact& operator=(PaintArtifact&& other) = delete;

  bool IsEmpty() const { return chunks_.IsEmpty(); }

  DisplayItemList& GetDisplayItemList() { return display_item_list_; }
  const DisplayItemList& GetDisplayItemList() const {
    return display_item_list_;
  }

  Vector<PaintChunk>& PaintChunks() { return chunks_; }
  const Vector<PaintChunk>& PaintChunks() const { return chunks_; }

  DisplayItemRange DisplayItemsInChunk(wtf_size_t chunk_index) const {
    DCHECK_LT(chunk_index, chunks_.size());
    auto& chunk = chunks_[chunk_index];
    return display_item_list_.ItemsInRange(chunk.begin_index, chunk.end_index);
  }

  // Returns the approximate memory usage, excluding memory likely to be
  // shared with the embedder after copying to cc::DisplayItemList.
  size_t ApproximateUnsharedMemoryUsage() const;

  sk_sp<PaintRecord> GetPaintRecord(
      const PropertyTreeState& replay_state) const;

  void RecordDebugInfo(DisplayItemClientId, const String&, DOMNodeId);
  // Note that ClientDebugName() returns the debug name at the time the client
  // was last painted, which may be out-of-date for a client whose debug name
  // has changed, but not in a way that caused it to be repainted.  This can
  // happen, for example, when the 'id' or 'class' attribute on a DOM element
  // changes, but the change doesn't cause a style invalidation.
  String ClientDebugName(DisplayItemClientId) const;
  DOMNodeId ClientOwnerNodeId(DisplayItemClientId) const;
  String IdAsString(const DisplayItem::Id& id) const;

 private:
  struct ClientDebugInfo {
    String name;
    DOMNodeId owner_node_id;
  };

  using DebugInfo = HashMap<DisplayItemClientId, ClientDebugInfo>;

  DisplayItemList display_item_list_;
  Vector<PaintChunk> chunks_;
  DebugInfo debug_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_H_

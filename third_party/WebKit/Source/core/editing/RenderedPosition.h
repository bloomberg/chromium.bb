/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderedPosition_h
#define RenderedPosition_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "core/layout/line/InlineBox.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class GraphicsLayer;
class LayoutPoint;
class LayoutUnit;
class LayoutObject;
struct CompositedSelectionBound;

class CORE_EXPORT RenderedPosition {
  STACK_ALLOCATED();

 public:
  RenderedPosition();
  explicit RenderedPosition(const VisiblePosition&);
  explicit RenderedPosition(const VisiblePositionInFlatTree&);
  RenderedPosition(const Position&, TextAffinity);
  RenderedPosition(const PositionInFlatTree&, TextAffinity);
  bool IsEquivalent(const RenderedPosition&) const;

  bool IsNull() const { return !layout_object_; }
  RootInlineBox* RootBox() { return inline_box_ ? &inline_box_->Root() : 0; }

  unsigned char BidiLevelOnLeft() const;
  unsigned char BidiLevelOnRight() const;
  RenderedPosition LeftBoundaryOfBidiRun(unsigned char bidi_level_of_run);
  RenderedPosition RightBoundaryOfBidiRun(unsigned char bidi_level_of_run);

  enum ShouldMatchBidiLevel { kMatchBidiLevel, kIgnoreBidiLevel };
  bool AtLeftBoundaryOfBidiRun() const {
    return AtLeftBoundaryOfBidiRun(kIgnoreBidiLevel, 0);
  }
  bool AtRightBoundaryOfBidiRun() const {
    return AtRightBoundaryOfBidiRun(kIgnoreBidiLevel, 0);
  }
  // The following two functions return true only if the current position is
  // at the end of the bidi run of the specified bidi embedding level.
  bool AtLeftBoundaryOfBidiRun(unsigned char bidi_level_of_run) const {
    return AtLeftBoundaryOfBidiRun(kMatchBidiLevel, bidi_level_of_run);
  }
  bool AtRightBoundaryOfBidiRun(unsigned char bidi_level_of_run) const {
    return AtRightBoundaryOfBidiRun(kMatchBidiLevel, bidi_level_of_run);
  }

  Position PositionAtLeftBoundaryOfBiDiRun() const;
  Position PositionAtRightBoundaryOfBiDiRun() const;

  IntRect AbsoluteRect(LayoutUnit* extra_width_to_end_of_line = 0) const;

  void PositionInGraphicsLayerBacking(CompositedSelectionBound&,
                                      bool selection_start) const;

  // Returns whether this position is not visible on the screen (because
  // clipped out).
  bool IsVisible(bool selection_start);

 private:
  bool operator==(const RenderedPosition&) const { return false; }
  explicit RenderedPosition(LayoutObject*, InlineBox*, int offset);

  InlineBox* PrevLeafChild() const;
  InlineBox* NextLeafChild() const;
  bool AtLeftmostOffsetInBox() const {
    return inline_box_ && offset_ == inline_box_->CaretLeftmostOffset();
  }
  bool AtRightmostOffsetInBox() const {
    return inline_box_ && offset_ == inline_box_->CaretRightmostOffset();
  }
  bool AtLeftBoundaryOfBidiRun(ShouldMatchBidiLevel,
                               unsigned char bidi_level_of_run) const;
  bool AtRightBoundaryOfBidiRun(ShouldMatchBidiLevel,
                                unsigned char bidi_level_of_run) const;

  void GetLocalSelectionEndpoints(bool selection_start,
                                  LayoutPoint& edge_top_in_layer,
                                  LayoutPoint& edge_bottom_in_layer,
                                  bool& is_text_direction_rtl) const;

  FloatPoint LocalToInvalidationBackingPoint(
      const LayoutPoint& local_point,
      GraphicsLayer** graphics_layer_backing) const;

  static LayoutPoint GetSamplePointForVisibility(
      const LayoutPoint& edge_top_in_layer,
      const LayoutPoint& edge_bottom_in_layer);

  LayoutObject* layout_object_;
  InlineBox* inline_box_;
  int offset_;

  static InlineBox* UncachedInlineBox() {
    return reinterpret_cast<InlineBox*>(1);
  }
  // Needs to be different form 0 so pick 1 because it's also on the null page.

  mutable InlineBox* prev_leaf_child_;
  mutable InlineBox* next_leaf_child_;

  FRIEND_TEST_ALL_PREFIXES(RenderedPositionTest, GetSamplePointForVisibility);
};

inline RenderedPosition::RenderedPosition()
    : layout_object_(nullptr),
      inline_box_(nullptr),
      offset_(0),
      prev_leaf_child_(UncachedInlineBox()),
      next_leaf_child_(UncachedInlineBox()) {}

inline RenderedPosition::RenderedPosition(LayoutObject* layout_object,
                                          InlineBox* box,
                                          int offset)
    : layout_object_(layout_object),
      inline_box_(box),
      offset_(offset),
      prev_leaf_child_(UncachedInlineBox()),
      next_leaf_child_(UncachedInlineBox()) {}

CORE_EXPORT bool LayoutObjectContainsPosition(LayoutObject*, const Position&);

}  // namespace blink

#endif  // RenderedPosition_h

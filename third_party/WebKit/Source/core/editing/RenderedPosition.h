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
#include "platform/wtf/Optional.h"

namespace blink {

class FrameSelection;
class LayoutObject;
struct CompositedSelection;

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
  const RootInlineBox* RootBox() const {
    return inline_box_ ? &inline_box_->Root() : nullptr;
  }

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

  // TODO(editing-dev): This function doesn't use RenderedPosition
  // instance anymore. Consider moving.
  static CompositedSelection ComputeCompositedSelection(const FrameSelection&);

 private:
  bool operator==(const RenderedPosition&) const { return false; }
  explicit RenderedPosition(const LayoutObject*, const InlineBox*, int offset);

  const InlineBox* PrevLeafChild() const;
  const InlineBox* NextLeafChild() const;
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

  const LayoutObject* layout_object_;
  const InlineBox* inline_box_;
  int offset_;

  mutable Optional<const InlineBox*> prev_leaf_child_;
  mutable Optional<const InlineBox*> next_leaf_child_;
};

inline RenderedPosition::RenderedPosition()
    : layout_object_(nullptr), inline_box_(nullptr), offset_(0) {}

inline RenderedPosition::RenderedPosition(const LayoutObject* layout_object,
                                          const InlineBox* box,
                                          int offset)
    : layout_object_(layout_object), inline_box_(box), offset_(offset) {}

CORE_EXPORT bool LayoutObjectContainsPosition(LayoutObject*, const Position&);

}  // namespace blink

#endif  // RenderedPosition_h

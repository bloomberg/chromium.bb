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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_RENDERED_POSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_RENDERED_POSITION_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/line/inline_box.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class FrameSelection;
struct CompositedSelection;

// TODO(xiaochengh): RenderedPosition is deprecated. It's currently only used in
// |SelectionController| for bidi adjustment. We should break this class and
// move relevant code to |inline_box_traversal.cc|, and generalize it so that it
// can be reused in LayoutNG.
class CORE_EXPORT RenderedPosition {
  STACK_ALLOCATED();

 public:
  RenderedPosition() = default;
  static RenderedPosition Create(const VisiblePositionInFlatTree&);

  bool IsNull() const { return !inline_box_; }
  bool operator==(const RenderedPosition& other) const {
    return inline_box_ == other.inline_box_ && offset_ == other.offset_ &&
           bidi_boundary_type_ == other.bidi_boundary_type_;
  }

  bool AtBidiBoundary() const {
    return bidi_boundary_type_ != BidiBoundaryType::kNotBoundary;
  }

  // Given |other|, which is a boundary of a bidi run, returns true if |this|
  // can be the other boundary of that run by checking some conditions.
  bool IsPossiblyOtherBoundaryOf(const RenderedPosition& other) const;

  // Callable only when |this| is at boundary of a bidi run. Returns true if
  // |other| is in that bidi run.
  bool BidiRunContains(const RenderedPosition& other) const;

  PositionInFlatTree GetPosition() const;

  // TODO(editing-dev): This function doesn't use RenderedPosition
  // instance anymore. Consider moving.
  static CompositedSelection ComputeCompositedSelection(const FrameSelection&);

 private:
  enum class BidiBoundaryType { kNotBoundary, kLeftBoundary, kRightBoundary };
  explicit RenderedPosition(const InlineBox*, int offset, BidiBoundaryType);

  const InlineBox* inline_box_ = nullptr;
  int offset_ = 0;
  BidiBoundaryType bidi_boundary_type_ = BidiBoundaryType::kNotBoundary;
};

inline RenderedPosition::RenderedPosition(const InlineBox* box,
                                          int offset,
                                          BidiBoundaryType type)
    : inline_box_(box), offset_(offset), bidi_boundary_type_(type) {}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_RENDERED_POSITION_H_

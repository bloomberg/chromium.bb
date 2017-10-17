// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBoxFragmentBuilder_h
#define NGLineBoxFragmentBuilder_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_container_fragment_builder.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class NGInlineNode;
class NGPhysicalFragment;

class CORE_EXPORT NGLineBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentBuilder(NGInlineNode,
                           RefPtr<const ComputedStyle>,
                           NGWritingMode);

  NGLogicalSize Size() const final;

  void MoveChildrenInBlockDirection(LayoutUnit);
  void MoveChildrenInBlockDirection(LayoutUnit, unsigned start, unsigned end);

  Vector<RefPtr<NGPhysicalFragment>>& MutableChildren() { return children_; }
  const Vector<NGLogicalOffset>& Offsets() const { return offsets_; }
  Vector<NGLogicalOffset>& MutableOffsets() { return offsets_; }

  void SetMetrics(const NGLineHeightMetrics&);
  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  void AddPositionedFloat(const NGPositionedFloat&);

  // Set the break token for the fragment to build.
  // A finished break token will be attached if not set.
  void SetBreakToken(RefPtr<NGInlineBreakToken>);

  // Creates the fragment. Can only be called once.
  RefPtr<NGLayoutResult> ToLineBoxFragment();

 private:
  NGInlineNode node_;

  NGLineHeightMetrics metrics_;
  Vector<NGPositionedFloat> positioned_floats_;

  RefPtr<NGInlineBreakToken> break_token_;
};

}  // namespace blink

#endif  // NGLineBoxFragmentBuilder

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBoxFragmentBuilder_h
#define NGLineBoxFragmentBuilder_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_line_height_metrics.h"
#include "wtf/Allocator.h"

namespace blink {

class NGInlineNode;
class NGPhysicalFragment;
class NGPhysicalLineBoxFragment;

class CORE_EXPORT NGLineBoxFragmentBuilder final {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentBuilder(NGInlineNode*);

  NGLineBoxFragmentBuilder& SetDirection(TextDirection);

  NGLineBoxFragmentBuilder& SetInlineSize(LayoutUnit);

  NGLineBoxFragmentBuilder& AddChild(RefPtr<NGPhysicalFragment>,
                                     const NGLogicalOffset&);
  void MoveChildrenInBlockDirection(LayoutUnit);

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  void UniteMetrics(const NGLineHeightMetrics&);
  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  // Creates the fragment. Can only be called once.
  RefPtr<NGPhysicalLineBoxFragment> ToLineBoxFragment();

 private:
  TextDirection direction_;

  Persistent<NGInlineNode> node_;

  LayoutUnit inline_size_;

  Vector<RefPtr<NGPhysicalFragment>> children_;
  Vector<NGLogicalOffset> offsets_;

  NGLineHeightMetrics metrics_;
};

}  // namespace blink

#endif  // NGLineBoxFragmentBuilder

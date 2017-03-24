// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragmentBuilder_h
#define NGTextFragmentBuilder_h

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_line_height_metrics.h"
#include "platform/text/TextDirection.h"
#include "wtf/Allocator.h"

namespace blink {

class NGInlineNode;
class NGPhysicalTextFragment;

class CORE_EXPORT NGTextFragmentBuilder final {
  STACK_ALLOCATED();

 public:
  NGTextFragmentBuilder(NGInlineNode*);

  NGTextFragmentBuilder& SetDirection(TextDirection);

  NGTextFragmentBuilder& SetInlineSize(LayoutUnit);
  NGTextFragmentBuilder& SetBlockSize(LayoutUnit);

  void UniteMetrics(const NGLineHeightMetrics&);
  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  // Creates the fragment. Can only be called once.
  RefPtr<NGPhysicalTextFragment> ToTextFragment(unsigned index,
                                                unsigned start_offset,
                                                unsigned end_offset);

 private:
  TextDirection direction_;

  Persistent<NGInlineNode> node_;

  NGLogicalSize size_;

  NGLineHeightMetrics metrics_;
};

}  // namespace blink

#endif  // NGTextFragmentBuilder

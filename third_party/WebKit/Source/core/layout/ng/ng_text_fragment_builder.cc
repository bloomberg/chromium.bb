// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_fragment_builder.h"

#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

NGTextFragmentBuilder::NGTextFragmentBuilder(NGInlineNode* node)
    : direction_(TextDirection::kLtr), node_(node) {}

NGTextFragmentBuilder& NGTextFragmentBuilder::SetDirection(
    TextDirection direction) {
  direction_ = direction;
  return *this;
}

NGTextFragmentBuilder& NGTextFragmentBuilder::SetInlineSize(LayoutUnit size) {
  size_.inline_size = size;
  return *this;
}

NGTextFragmentBuilder& NGTextFragmentBuilder::SetBlockSize(LayoutUnit size) {
  size_.block_size = size;
  return *this;
}

void NGTextFragmentBuilder::UniteMetrics(const NGLineHeightMetrics& metrics) {
  metrics_.Unite(metrics);
}

RefPtr<NGPhysicalTextFragment> NGTextFragmentBuilder::ToTextFragment(
    unsigned index,
    unsigned start_offset,
    unsigned end_offset) {
  NGWritingMode writing_mode(
      FromPlatformWritingMode(node_->Style().getWritingMode()));
  return adoptRef(new NGPhysicalTextFragment(
      node_->GetLayoutObject(), node_, index, start_offset, end_offset,
      size_.ConvertToPhysical(writing_mode)));
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping_builder.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

NGOffsetMappingBuilder::NGOffsetMappingBuilder() {
  mapping_.push_back(0);
}

void NGOffsetMappingBuilder::AppendIdentityMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  DCHECK(!mapping_.IsEmpty());
  for (unsigned i = 0; i < length; ++i) {
    unsigned next = mapping_.back() + 1;
    mapping_.push_back(next);
  }
}

void NGOffsetMappingBuilder::AppendCollapsedMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  DCHECK(!mapping_.IsEmpty());
  const unsigned back = mapping_.back();
  for (unsigned i = 0; i < length; ++i)
    mapping_.push_back(back);
}

void NGOffsetMappingBuilder::CollapseTrailingSpace(unsigned skip_length) {
  DCHECK(!mapping_.IsEmpty());

  // Find the |skipped_count + 1|-st last uncollapsed character. By collapsing
  // it, all mapping values beyond this position are decremented by 1.
  unsigned skipped_count = 0;
  for (unsigned i = mapping_.size() - 1; skipped_count <= skip_length; --i) {
    DCHECK_GT(i, 0u);
    if (mapping_[i] != mapping_[i - 1])
      ++skipped_count;
    --mapping_[i];
  }
}

Vector<unsigned> NGOffsetMappingBuilder::DumpOffsetMappingForTesting() const {
  return mapping_;
}

}  // namespace blink

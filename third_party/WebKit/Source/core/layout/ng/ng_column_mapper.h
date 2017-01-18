// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGColumnMapper_h
#define NGColumnMapper_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "platform/heap/Handle.h"

namespace blink {

// Keeps track of the current column, and is used to map offsets from the
// coordinate established by the fragmented flow [1] to visual coordinates.
//
// [1] https://drafts.csswg.org/css-break/#fragmented-flow
//
// TODO(mstensho): Add a NGFragmentainerMapper super class. There are other
// fragmentation types than multicol (regions, pagination for printing).
class CORE_EXPORT NGColumnMapper
    : public GarbageCollectedFinalized<NGColumnMapper> {
 public:
  NGColumnMapper(LayoutUnit inline_progression, LayoutUnit block_size)
      : inline_progression_(inline_progression), block_size_(block_size) {}

  // Column block length.
  LayoutUnit BlockSize() const { return block_size_; }

  // Offset for next break, in the fragmented flow coordinate space.
  LayoutUnit NextBreakOffset() const {
    return last_break_block_offset_ + block_size_;
  }

  // Convert an offset in the fragmented flow coordinate space to the visual
  // coordinate space.
  void ToVisualOffset(NGLogicalOffset& offset) {
    offset.inline_offset += inline_offset_;
    offset.block_offset -= last_break_block_offset_;
  }

  // Specify where in the content to start layout of the next column.
  void SetBreakToken(NGBlockBreakToken* token) {
    DCHECK(!break_token_);
    break_token_ = token;
  }
  bool HasBreakToken() const { return break_token_; }

  // Advance to the next column. Returns the break token to resume at. If
  // nullptr is returned, there's no more content to process.
  NGBlockBreakToken* Advance() {
    NGBlockBreakToken* token = break_token_;
    break_token_ = nullptr;
    if (token) {
      inline_offset_ += inline_progression_;
      last_break_block_offset_ += block_size_;
    }
    return token;
  }

  DEFINE_INLINE_TRACE() { visitor->trace(break_token_); }

 private:
  // Where to resume in the next column.
  Member<NGBlockBreakToken> break_token_;

  // The sum of used column-width and column_gap.
  LayoutUnit inline_progression_;

  // Inline offset to the current column.
  LayoutUnit inline_offset_;

  // Block size of a column.
  LayoutUnit block_size_;

  // Amount of content held by previous columns.
  LayoutUnit last_break_block_offset_;
};

}  // namespace blink

#endif  // NGColumnMapper_h

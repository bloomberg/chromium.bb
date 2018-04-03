// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextOffsetMapping_h
#define TextOffsetMapping_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Forward.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "platform/heap/Member.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LayoutBlock;

// Mapping between position and text offset in |LayoutBlock| == CSS Block
// with using characters from |TextIterator|.
//
// This class is similar to |NGOffsetMapping| which uses |text_offset_| in
// |NGInlineNodeData| except for
//  - Treats characters with CSS property "-webkit-text-security" as "x"
//    instead of a bullet (U+2022), which breaks words.
//  - Contains characters in float and inline-blocks. |NGOffsetMapping| treats
//    them as one object replacement character (U+FFFC). See |CollectInlines|
//    in |NGInlineNode|.
class CORE_EXPORT TextOffsetMapping final {
  STACK_ALLOCATED();

 public:
  // Constructor |TextOffsetMapping| for specified |LayoutBlock|.
  explicit TextOffsetMapping(const LayoutBlock&);

  ~TextOffsetMapping() = default;

  // Returns range of |LayoutBlock|.
  const EphemeralRangeInFlatTree GetRange() const { return range_; }

  // Returns characters in subtree of |LayoutBlock|, collapsed whitespaces
  // are not included.
  const String& GetText() const { return text16_; }

  // Returns offset in |text16_| of specified position.
  int ComputeTextOffset(const PositionInFlatTree&) const;

  // Returns position before |offset| in |text16_|
  PositionInFlatTree GetPositionBefore(unsigned offset) const;

  // Returns position after |offset| in |text16_|
  PositionInFlatTree GetPositionAfter(unsigned offset) const;

  // Returns a range specified by |start| and |end| offset in |text16_|.
  EphemeralRangeInFlatTree ComputeRange(unsigned start, unsigned end) const;

  // Returns an offset in |text16_| before non-whitespace character from
  // |offset|, inclusive, otherwise returns |text16_.length()|.
  // This function is used for computing trailing whitespace after word.
  unsigned FindNonWhitespaceCharacterFrom(unsigned offset) const;

  // Helper functions to constructor |TextOffsetMapping|.
  static const LayoutBlock& ComputeContainigBlock(const PositionInFlatTree&);
  static LayoutBlock* NextBlockFor(const LayoutBlock&);
  static LayoutBlock* PreviousBlockFor(const LayoutBlock&);

 private:
  TextOffsetMapping(const LayoutBlock&, const TextIteratorBehavior);

  const TextIteratorBehavior behavior_;
  const EphemeralRangeInFlatTree range_;
  const String text16_;

  DISALLOW_COPY_AND_ASSIGN(TextOffsetMapping);
};

}  // namespace blink

#endif  // TextOffsetMapping_h

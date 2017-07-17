// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_item.h"

#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/shaping/ShapeResultBuffer.h"

namespace blink {
namespace {

const char* kNGInlineItemTypeStrings[] = {
    "Text",     "Control",  "AtomicInline",        "OpenTag",
    "CloseTag", "Floating", "OutOfFlowPositioned", "BidiControl"};

}  // namespace

const char* NGInlineItem::NGInlineItemTypeToString(int val) const {
  return kNGInlineItemTypeStrings[val];
}

// Set bidi level to a list of NGInlineItem from |index| to the item that ends
// with |end_offset|.
// If |end_offset| is mid of an item, the item is split to ensure each item has
// one bidi level.
// @param items The list of NGInlineItem.
// @param index The first index of the list to set.
// @param end_offset The exclusive end offset to set.
// @param level The level to set.
// @return The index of the next item.
unsigned NGInlineItem::SetBidiLevel(Vector<NGInlineItem>& items,
                                    unsigned index,
                                    unsigned end_offset,
                                    UBiDiLevel level) {
  for (; items[index].end_offset_ < end_offset; index++)
    items[index].bidi_level_ = level;
  items[index].bidi_level_ = level;

  if (items[index].end_offset_ == end_offset) {
    // Let close items have the same bidi-level as the previous item.
    while (index + 1 < items.size() &&
           items[index + 1].Type() == NGInlineItem::kCloseTag) {
      items[++index].bidi_level_ = level;
    }
  } else {
    Split(items, index, end_offset);
  }

  return index + 1;
}

String NGInlineItem::ToString() const {
  return String::Format("NGInlineItem. Type: '%s'. LayoutObject: '%s'",
                        NGInlineItemTypeToString(Type()),
                        GetLayoutObject()->DebugName().Ascii().data());
}

// Split |items[index]| to 2 items at |offset|.
// All properties other than offsets are copied to the new item and it is
// inserted at |items[index + 1]|.
// @param items The list of NGInlineItem.
// @param index The index to split.
// @param offset The offset to split at.
void NGInlineItem::Split(Vector<NGInlineItem>& items,
                         unsigned index,
                         unsigned offset) {
  DCHECK_GT(offset, items[index].start_offset_);
  DCHECK_LT(offset, items[index].end_offset_);
  items.insert(index + 1, items[index]);
  items[index].end_offset_ = offset;
  items[index + 1].start_offset_ = offset;
}

void NGInlineItem::SetOffset(unsigned start, unsigned end) {
  DCHECK_GE(end, start);
  start_offset_ = start;
  end_offset_ = end;
}

void NGInlineItem::SetEndOffset(unsigned end_offset) {
  DCHECK_GE(end_offset, start_offset_);
  end_offset_ = end_offset;
}

LayoutUnit NGInlineItem::InlineSize() const {
  if (Type() == NGInlineItem::kText)
    return LayoutUnit(shape_result_->Width());

  DCHECK_NE(Type(), NGInlineItem::kAtomicInline)
      << "Use NGInlineLayoutAlgorithm::InlineSize";
  // Bidi controls and out-of-flow objects do not have in-flow widths.
  return LayoutUnit();
}

LayoutUnit NGInlineItem::InlineSize(unsigned start, unsigned end) const {
  DCHECK_GE(start, StartOffset());
  DCHECK_LE(start, end);
  DCHECK_LE(end, EndOffset());

  if (start == end)
    return LayoutUnit();
  if (start == start_offset_ && end == end_offset_)
    return InlineSize();

  DCHECK_EQ(Type(), NGInlineItem::kText);
  return LayoutUnit(ShapeResultBuffer::GetCharacterRange(
                        shape_result_, Direction(), shape_result_->Width(),
                        start - StartOffset(), end - StartOffset())
                        .Width());
}

bool NGInlineItem::HasStartEdge() const {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  // TODO(kojii): Should use break token when NG has its own tree building.
  return !GetLayoutObject()->IsInlineElementContinuation();
}

bool NGInlineItem::HasEndEdge() const {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  // TODO(kojii): Should use break token when NG has its own tree building.
  return !ToLayoutInline(GetLayoutObject())->Continuation();
}

NGInlineItemRange::NGInlineItemRange(Vector<NGInlineItem>* items,
                                     unsigned start_index,
                                     unsigned end_index)
    : start_item_(&(*items)[start_index]),
      size_(end_index - start_index),
      start_index_(start_index) {
  CHECK_LE(start_index, end_index);
  CHECK_LE(end_index, items->size());
}

}  // namespace blink

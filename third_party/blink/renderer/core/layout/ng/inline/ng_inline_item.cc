// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"

namespace blink {
namespace {

const char* kNGInlineItemTypeStrings[] = {
    "Text",     "Control",  "AtomicInline",        "OpenTag",
    "CloseTag", "Floating", "OutOfFlowPositioned", "BidiControl"};

// Returns true if this inline box is "empty", i.e. if the node contains only
// empty items it will produce a single zero block-size line box.
//
// While the spec defines "non-zero margins, padding, or borders" prevents
// line boxes to be zero-height, tests indicate that only inline direction
// of them do so. https://drafts.csswg.org/css2/visuren.html
bool IsInlineBoxEmpty(const ComputedStyle& style,
                      const LayoutObject& layout_object) {
  if (style.BorderStart().NonZero() || !style.PaddingStart().IsZero() ||
      style.BorderEnd().NonZero() || !style.PaddingEnd().IsZero())
    return false;

  // Non-zero margin can prevent "empty" only in non-quirks mode.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  if ((!style.MarginStart().IsZero() || !style.MarginEnd().IsZero()) &&
      !layout_object.GetDocument().InLineHeightQuirksMode())
    return false;

  return true;
}

}  // namespace

NGInlineItem::NGInlineItem(NGInlineItemType type,
                           unsigned start,
                           unsigned end,
                           const ComputedStyle* style,
                           LayoutObject* layout_object)
    : start_offset_(start),
      end_offset_(end),
      script_(USCRIPT_INVALID_CODE),
      style_(style),
      layout_object_(layout_object),
      type_(type),
      bidi_level_(UBIDI_LTR),
      shape_options_(kPreContext | kPostContext),
      is_empty_item_(false),
      should_create_box_fragment_(false),
      style_variant_(static_cast<unsigned>(NGStyleVariant::kStandard)),
      end_collapse_type_(kNotCollapsible) {
  DCHECK_GE(end, start);
  ComputeBoxProperties();
}

NGInlineItem::~NGInlineItem() = default;

void NGInlineItem::ComputeBoxProperties() {
  DCHECK(!is_empty_item_);
  DCHECK(!should_create_box_fragment_);

  if (type_ == NGInlineItem::kText || type_ == NGInlineItem::kAtomicInline ||
      type_ == NGInlineItem::kControl)
    return;

  if (type_ == NGInlineItem::kOpenTag) {
    DCHECK(style_ && layout_object_ && layout_object_->IsLayoutInline());
    if (layout_object_->HasBoxDecorationBackground() || style_->HasPadding() ||
        style_->HasMargin()) {
      is_empty_item_ = IsInlineBoxEmpty(*style_, *layout_object_);
      should_create_box_fragment_ = true;
    } else {
      is_empty_item_ = true;
      should_create_box_fragment_ =
          ToLayoutBoxModelObject(layout_object_)->HasSelfPaintingLayer() ||
          style_->HasOutline() || style_->CanContainAbsolutePositionObjects() ||
          style_->CanContainFixedPositionObjects();
    }
    return;
  }

  if (type_ == kListMarker) {
    is_empty_item_ = false;
    return;
  }

  is_empty_item_ = true;
}

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

UBiDiLevel NGInlineItem::BidiLevelForReorder() const {
  // List markers should not be reordered to protect it from being included into
  // unclosed inline boxes.
  return Type() != NGInlineItem::kListMarker ? BidiLevel() : 0;
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

bool NGInlineItem::HasStartEdge() const {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  // TODO(kojii): Should use break token when NG has its own tree building.
  return !GetLayoutObject()->IsInlineElementContinuation();
}

bool NGInlineItem::HasEndEdge() const {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  // TODO(kojii): Should use break token when NG has its own tree building.
  return !GetLayoutObject()->IsLayoutInline() ||
         !ToLayoutInline(GetLayoutObject())->Continuation();
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

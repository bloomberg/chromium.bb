// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
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
bool IsInlineBoxStartEmpty(const ComputedStyle& style,
                           const LayoutObject& layout_object) {
  if (style.BorderStartWidth() || !style.PaddingStart().IsZero())
    return false;

  // Non-zero margin can prevent "empty" only in non-quirks mode.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  if (!style.MarginStart().IsZero() &&
      !layout_object.GetDocument().InLineHeightQuirksMode())
    return false;

  return true;
}

// Determines if the end of a box is "empty" as defined above.
//
// Keeping the "empty" state for start and end separately is important when they
// belong to different lines, as non-empty item can force the line it belongs to
// as non-empty.
bool IsInlineBoxEndEmpty(const ComputedStyle& style,
                         const LayoutObject& layout_object) {
  if (style.BorderEndWidth() || !style.PaddingEnd().IsZero())
    return false;

  // Non-zero margin can prevent "empty" only in non-quirks mode.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  if (!style.MarginEnd().IsZero() &&
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
      style_(style),
      layout_object_(layout_object),
      type_(type),
      segment_data_(0),
      bidi_level_(UBIDI_LTR),
      shape_options_(kPreContext | kPostContext),
      is_empty_item_(false),
      style_variant_(static_cast<unsigned>(NGStyleVariant::kStandard)),
      end_collapse_type_(kNotCollapsible),
      is_end_collapsible_newline_(false),
      is_symbol_marker_(false),
      is_generated_(false) {
  DCHECK_GE(end, start);
  ComputeBoxProperties();
}

NGInlineItem::NGInlineItem(const NGInlineItem& other,
                           unsigned start,
                           unsigned end,
                           scoped_refptr<const ShapeResult> shape_result)
    : start_offset_(start),
      end_offset_(end),
      shape_result_(shape_result),
      style_(other.style_),
      layout_object_(other.layout_object_),
      type_(other.type_),
      segment_data_(other.segment_data_),
      bidi_level_(other.bidi_level_),
      shape_options_(other.shape_options_),
      is_empty_item_(other.is_empty_item_),
      style_variant_(other.style_variant_),
      end_collapse_type_(other.end_collapse_type_),
      is_end_collapsible_newline_(other.is_end_collapsible_newline_),
      is_symbol_marker_(other.is_symbol_marker_),
      is_generated_(other.is_generated_) {
  DCHECK_GE(end, start);
}

NGInlineItem::~NGInlineItem() = default;

bool NGInlineItem::ShouldCreateBoxFragment() const {
  if (Type() == kOpenTag || Type() == kCloseTag)
    return ToLayoutInline(layout_object_)->ShouldCreateBoxFragment();
  DCHECK_EQ(Type(), kAtomicInline);
  return false;
}

void NGInlineItem::SetShouldCreateBoxFragment() {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  ToLayoutInline(layout_object_)->SetShouldCreateBoxFragment();
}

void NGInlineItem::ComputeBoxProperties() {
  DCHECK(!is_empty_item_);

  if (type_ == NGInlineItem::kText || type_ == NGInlineItem::kAtomicInline ||
      type_ == NGInlineItem::kControl)
    return;

  if (type_ == NGInlineItem::kOpenTag) {
    DCHECK(style_ && layout_object_ && layout_object_->IsLayoutInline());
    is_empty_item_ = IsInlineBoxStartEmpty(*style_, *layout_object_);
    return;
  }

  if (type_ == NGInlineItem::kCloseTag) {
    DCHECK(style_ && layout_object_ && layout_object_->IsLayoutInline());
    is_empty_item_ = IsInlineBoxEndEmpty(*style_, *layout_object_);
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

RunSegmenter::RunSegmenterRange NGInlineItem::CreateRunSegmenterRange() const {
  return NGInlineItemSegment::UnpackSegmentData(start_offset_, end_offset_,
                                                segment_data_);
}

bool NGInlineItem::EqualsRunSegment(const NGInlineItem& other) const {
  return segment_data_ == other.segment_data_;
}

void NGInlineItem::SetSegmentData(unsigned segment_data) {
  DCHECK_EQ(Type(), NGInlineItem::kText);
  segment_data_ = segment_data;
}

void NGInlineItem::SetSegmentData(const RunSegmenter::RunSegmenterRange& range,
                                  Vector<NGInlineItem>* items) {
  unsigned segment_data = NGInlineItemSegment::PackSegmentData(range);
  for (NGInlineItem& item : *items) {
    if (item.Type() == NGInlineItem::kText)
      item.segment_data_ = segment_data;
  }
}

void NGInlineItem::SetBidiLevel(UBiDiLevel level) {
  // Invalidate ShapeResult because it depends on the resolved direction.
  if (DirectionFromLevel(level) != DirectionFromLevel(bidi_level_))
    shape_result_ = nullptr;
  bidi_level_ = level;
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
    items[index].SetBidiLevel(level);
  items[index].SetBidiLevel(level);

  if (items[index].end_offset_ == end_offset) {
    // Let close items have the same bidi-level as the previous item.
    while (index + 1 < items.size() &&
           items[index + 1].Type() == NGInlineItem::kCloseTag) {
      items[++index].SetBidiLevel(level);
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
  items[index].shape_result_ = nullptr;
  items.insert(index + 1, items[index]);
  items[index].end_offset_ = offset;
  items[index + 1].start_offset_ = offset;
}

void NGInlineItem::SetOffset(unsigned start, unsigned end) {
  DCHECK_GE(end, start);
  start_offset_ = start;
  end_offset_ = end;
  // Any modification to the offset will invalidate the shape result.
  shape_result_ = nullptr;
}

void NGInlineItem::SetEndOffset(unsigned end_offset) {
  DCHECK_GE(end_offset, start_offset_);
  end_offset_ = end_offset;
  // Any modification to the offset will invalidate the shape result.
  shape_result_ = nullptr;
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

void NGInlineItem::SetEndCollapseType(NGCollapseType type) {
  DCHECK(Type() == NGInlineItem::kText || type == kOpaqueToCollapsing ||
         (Type() == NGInlineItem::kControl && type == kCollapsible));
  end_collapse_type_ = type;
}

void NGInlineItem::SetEndCollapseType(NGCollapseType type, bool is_newline) {
  SetEndCollapseType(type);
  is_end_collapsible_newline_ = is_newline;
}

}  // namespace blink

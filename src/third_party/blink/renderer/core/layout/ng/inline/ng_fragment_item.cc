// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGFragmentItem::NGFragmentItem(const NGPhysicalTextFragment& text)
    : layout_object_(text.GetLayoutObject()),
      text_({text.TextShapeResult(), text.StartOffset(), text.EndOffset()}),
      rect_({PhysicalOffset(), text.Size()}),
      type_(kText),
      style_variant_(static_cast<unsigned>(text.StyleVariant())),
      is_hidden_for_paint_(false) {
  DCHECK_LE(text_.start_offset, text_.end_offset);
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), text_.start_offset);
    DCHECK_EQ(text_.shape_result->EndIndex(), text_.end_offset);
  }
#endif
}

NGFragmentItem::NGFragmentItem(const NGPhysicalLineBoxFragment& line,
                               wtf_size_t item_count)
    : layout_object_(nullptr),
      line_({line.Metrics(), To<NGInlineBreakToken>(line.BreakToken()),
             item_count}),
      rect_({PhysicalOffset(), line.Size()}),
      type_(kLine),
      style_variant_(static_cast<unsigned>(line.StyleVariant())),
      is_hidden_for_paint_(false) {}

NGFragmentItem::NGFragmentItem(const NGPhysicalBoxFragment& box,
                               wtf_size_t item_count)
    : layout_object_(box.GetLayoutObject()),
      box_({&box, item_count}),
      rect_({PhysicalOffset(), box.Size()}),
      type_(kBox),
      style_variant_(static_cast<unsigned>(box.StyleVariant())),
      is_hidden_for_paint_(false) {}

NGFragmentItem::~NGFragmentItem() {
  switch (Type()) {
    case kText:
      text_.~TextItem();
      break;
    case kGeneratedText:
      generated_text_.~GeneratedTextItem();
      break;
    case kLine:
      line_.~LineItem();
      break;
    case kBox:
      box_.~BoxItem();
      break;
  }
}

PhysicalRect NGFragmentItem::SelfInkOverflow() const {
  // TODO(kojii): Implement.
  return LocalRect();
}

StringView NGFragmentItem::Text(const NGFragmentItems& items) const {
  if (Type() == kText) {
    DCHECK_LE(text_.start_offset, text_.end_offset);
    return StringView(items.Text(UsesFirstLineStyle()), text_.start_offset,
                      text_.end_offset - text_.start_offset);
  }
  NOTREACHED();
  return StringView();
}

NGTextFragmentPaintInfo NGFragmentItem::TextPaintInfo(
    const NGFragmentItems& items) const {
  if (Type() == kText) {
    return {items.Text(UsesFirstLineStyle()), text_.start_offset,
            text_.end_offset, text_.shape_result.get()};
  }
  NOTREACHED();
  return {};
}

String NGFragmentItem::DebugName() const {
  return "NGFragmentItem";
}

IntRect NGFragmentItem::VisualRect() const {
  // TODO(kojii): Need to reconsider the storage of |VisualRect|, to integrate
  // better with |FragmentData| and to avoid dependency to |LayoutObject|.
  return GetLayoutObject()->VisualRectForInlineBox();
}

PhysicalRect NGFragmentItem::LocalVisualRectFor(
    const LayoutObject& layout_object) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());

  PhysicalRect visual_rect;
  for (const NGFragmentItem& item : ItemsFor(layout_object)) {
    if (UNLIKELY(item.IsHiddenForPaint()))
      continue;
    PhysicalRect child_visual_rect = item.SelfInkOverflow();
    child_visual_rect.offset += item.Offset();
    visual_rect.Unite(child_visual_rect);
  }
  return visual_rect;
}

NGFragmentItem::ItemsForLayoutObject NGFragmentItem::ItemsFor(
    const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  DCHECK(layout_object.IsText() || layout_object.IsLayoutInline() ||
         (layout_object.IsBox() && layout_object.IsInline()));

  // TODO(kojii): This is a hot function needed by paint and several other
  // operations. Make this fast, by not iterating.
  if (const LayoutBlockFlow* block_flow =
          layout_object.RootInlineFormattingContext()) {
    if (const NGPhysicalBoxFragment* fragment = block_flow->CurrentFragment()) {
      if (const NGFragmentItems* items = fragment->Items()) {
        for (unsigned i = 0; i < items->Items().size(); ++i) {
          const NGFragmentItem* item = items->Items()[i].get();
          if (item->GetLayoutObject() == &layout_object)
            return ItemsForLayoutObject(items->Items(), i, item);
        }
      }
    }
  }

  return ItemsForLayoutObject();
}

NGFragmentItem::ItemsForLayoutObject::Iterator&
NGFragmentItem::ItemsForLayoutObject::Iterator::operator++() {
  // TODO(kojii): This is a hot function needed by paint and several other
  // operations. Make this fast, by not iterating.
  if (!current_)
    return *this;
  const LayoutObject* current_layout_object = current_->GetLayoutObject();
  while (++index_ < items_->size()) {
    current_ = (*items_)[index_].get();
    if (current_->GetLayoutObject() == current_layout_object)
      return *this;
  }
  current_ = nullptr;
  return *this;
}

}  // namespace blink

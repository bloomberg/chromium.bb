// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_abstract_inline_text_box.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

// Mapping from NGFragmentItem/NGPaintFragment to NGAbstractInlineTextBox
// TODO(yosin): Once we get rid of |NGPaintFragment|, we should not use
// template class for |NGAbstractInlineTextBoxCache|.
template <typename Fragment>
class NGAbstractInlineTextBoxCache final {
 public:
  static scoped_refptr<AbstractInlineTextBox> GetOrCreate(
      const Fragment& fragment) {
    if (!s_instance_)
      s_instance_ = new NGAbstractInlineTextBoxCache();
    return s_instance_->GetOrCreateInternal(fragment);
  }

  static void WillDestroy(const Fragment* fragment) {
    if (!s_instance_)
      return;
    s_instance_->WillDestroyInternal(fragment);
  }

 private:
  scoped_refptr<AbstractInlineTextBox> GetOrCreateInternal(
      const Fragment& fragment) {
    const auto it = map_.find(&fragment);
    LayoutText* const layout_text =
        ToLayoutText(fragment.GetMutableLayoutObject());
    if (it != map_.end()) {
      CHECK(layout_text->HasAbstractInlineTextBox());
      return it->value;
    }
    scoped_refptr<AbstractInlineTextBox> obj = base::AdoptRef(
        new NGAbstractInlineTextBox(LineLayoutText(layout_text), fragment));
    map_.Set(&fragment, obj);
    layout_text->SetHasAbstractInlineTextBox();
    return obj;
  }

  void WillDestroyInternal(const Fragment* fragment) {
    const auto it = map_.find(fragment);
    if (it == map_.end())
      return;
    it->value->Detach();
    map_.erase(fragment);
  }

  static NGAbstractInlineTextBoxCache* s_instance_;

  HashMap<const Fragment*, scoped_refptr<AbstractInlineTextBox>> map_;
};

template <typename Fragment>
NGAbstractInlineTextBoxCache<Fragment>*
    NGAbstractInlineTextBoxCache<Fragment>::s_instance_ = nullptr;

}  // namespace

scoped_refptr<AbstractInlineTextBox> NGAbstractInlineTextBox::GetOrCreate(
    const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment()) {
    return NGAbstractInlineTextBoxCache<NGPaintFragment>::GetOrCreate(
        *paint_fragment);
  }
  if (const NGFragmentItem* fragment_item = cursor.CurrentItem()) {
    return NGAbstractInlineTextBoxCache<NGFragmentItem>::GetOrCreate(
        *fragment_item);
  }
  return nullptr;
}

void NGAbstractInlineTextBox::WillDestroy(const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment()) {
    return NGAbstractInlineTextBoxCache<NGPaintFragment>::WillDestroy(
        paint_fragment);
  }
  if (const NGFragmentItem* fragment_item = cursor.CurrentItem()) {
    return NGAbstractInlineTextBoxCache<NGFragmentItem>::WillDestroy(
        fragment_item);
  }
  NOTREACHED();
}

void NGAbstractInlineTextBox::WillDestroy(const NGPaintFragment* fragment) {
  NGAbstractInlineTextBoxCache<NGPaintFragment>::WillDestroy(fragment);
}

NGAbstractInlineTextBox::NGAbstractInlineTextBox(
    LineLayoutText line_layout_item,
    const NGPaintFragment& fragment)
    : AbstractInlineTextBox(line_layout_item), fragment_(&fragment) {
  DCHECK(fragment_->PhysicalFragment().IsText()) << fragment_;
}

NGAbstractInlineTextBox::NGAbstractInlineTextBox(
    LineLayoutText line_layout_item,
    const NGFragmentItem& fragment_item)
    : AbstractInlineTextBox(line_layout_item), fragment_item_(&fragment_item) {
  DCHECK(fragment_item_->IsText()) << fragment_item_;
}

NGAbstractInlineTextBox::~NGAbstractInlineTextBox() {
  DCHECK(!fragment_);
}

void NGAbstractInlineTextBox::Detach() {
  if (Node* const node = GetNode()) {
    if (AXObjectCache* cache = node->GetDocument().ExistingAXObjectCache())
      cache->InlineTextBoxesUpdated(GetLineLayoutItem());
  }
  AbstractInlineTextBox::Detach();
  fragment_ = nullptr;
}

NGInlineCursor NGAbstractInlineTextBox::GetCursor() const {
  if (!fragment_item_)
    return NGInlineCursor();
  NGInlineCursor cursor;
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    cursor.MoveTo(*fragment_item_);
  else
    cursor.MoveTo(*fragment_);
  DCHECK(!cursor.Current().GetLayoutObject()->NeedsLayout());
  return cursor;
}

NGInlineCursor NGAbstractInlineTextBox::GetCursorOnLine() const {
  NGInlineCursor current = GetCursor();
  NGInlineCursor line_box = current;
  line_box.MoveToContainingLine();
  NGInlineCursor cursor = line_box.CursorForDescendants();
  cursor.MoveTo(current);
  return cursor;
}

String NGAbstractInlineTextBox::GetTextContent() const {
  const NGInlineCursor& cursor = GetCursor();
  if (cursor.Current().IsLayoutGeneratedText())
    return cursor.Current().Text(cursor).ToString();
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment()) {
    return To<NGPhysicalTextFragment>(paint_fragment->PhysicalFragment())
        .TextContent();
  }
  return cursor.Items().Text(cursor.Current().UsesFirstLineStyle());
}

bool NGAbstractInlineTextBox::NeedsTrailingSpace() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor.Current().Style().CollapseWhiteSpace())
    return false;
  NGInlineCursor line_box = cursor;
  line_box.MoveToContainingLine();
  if (!line_box.Current().HasSoftWrapToNextLine())
    return false;
  const String text_content = GetTextContent();
  const unsigned end_offset = cursor.Current().TextEndOffset();
  if (end_offset >= text_content.length())
    return false;
  if (text_content[end_offset] != ' ')
    return false;
  const NGInlineBreakToken* break_token = line_box.Current().InlineBreakToken();
  DCHECK(break_token);
  // TODO(yosin): We should support OOF fragments between |fragment_| and
  // break token.
  if (break_token->TextOffset() != end_offset + 1)
    return false;
  // Check a character in text content after |fragment_| comes from same
  // layout text of |fragment_|.
  const LayoutObject* const layout_object = cursor.Current().GetLayoutObject();
  const NGOffsetMapping* mapping = NGOffsetMapping::GetFor(layout_object);
  // TODO(kojii): There's not much we can do for dirty-tree. crbug.com/946004
  if (!mapping)
    return false;
  const base::span<const NGOffsetMappingUnit> mapping_units =
      mapping->GetMappingUnitsForTextContentOffsetRange(end_offset,
                                                        end_offset + 1);
  if (mapping_units.begin() == mapping_units.end())
    return false;
  const NGOffsetMappingUnit& mapping_unit = mapping_units.front();
  return mapping_unit.GetLayoutObject() == layout_object;
}

scoped_refptr<AbstractInlineTextBox>
NGAbstractInlineTextBox::NextInlineTextBox() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return nullptr;
  NGInlineCursor next;
  next.MoveTo(*cursor.Current().GetLayoutObject());
  while (next != cursor)
    next.MoveToNextForSameLayoutObject();
  next.MoveToNextForSameLayoutObject();
  if (!next)
    return nullptr;
  return GetOrCreate(next);
}

LayoutRect NGAbstractInlineTextBox::LocalBounds() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return LayoutRect();
  return cursor.Current().RectInContainerBlock().ToLayoutRect();
}

unsigned NGAbstractInlineTextBox::Len() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return 0;
  if (NeedsTrailingSpace())
    return cursor.Current().Text(cursor).length() + 1;
  return cursor.Current().Text(cursor).length();
}

unsigned NGAbstractInlineTextBox::TextOffsetInContainer(unsigned offset) const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return 0;
  return cursor.Current().TextStartOffset() + offset;
}

AbstractInlineTextBox::Direction NGAbstractInlineTextBox::GetDirection() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return kLeftToRight;
  const TextDirection text_direction = cursor.Current().ResolvedDirection();
  if (GetLineLayoutItem().Style()->IsHorizontalWritingMode())
    return IsLtr(text_direction) ? kLeftToRight : kRightToLeft;
  return IsLtr(text_direction) ? kTopToBottom : kBottomToTop;
}

void NGAbstractInlineTextBox::CharacterWidths(Vector<float>& widths) const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return;
  const ShapeResultView* shape_result_view = cursor.Current().TextShapeResult();
  if (!shape_result_view) {
    // When |fragment_| for BR, we don't have shape result.
    // "aom-computed-boolean-properties.html" reaches here.
    widths.resize(Len());
    return;
  }
  // TODO(layout-dev): Add support for IndividualCharacterRanges to
  // ShapeResultView to avoid the copy below.
  scoped_refptr<ShapeResult> shape_result =
      shape_result_view->CreateShapeResult();
  Vector<CharacterRange> ranges;
  shape_result->IndividualCharacterRanges(&ranges);
  widths.ReserveCapacity(ranges.size());
  widths.resize(0);
  for (const auto& range : ranges)
    widths.push_back(range.Width());
  // The shaper can fail to return glyph metrics for all characters (see
  // crbug.com/613915 and crbug.com/615661) so add empty ranges to ensure all
  // characters have an associated range.
  widths.resize(Len());
}

String NGAbstractInlineTextBox::GetText() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return g_empty_string;

  String result = cursor.Current().Text(cursor).ToString();

  // For compatibility with |InlineTextBox|, we should have a space character
  // for soft line break.
  // Following tests require this:
  //  - accessibility/inline-text-change-style.html
  //  - accessibility/inline-text-changes.html
  //  - accessibility/inline-text-word-boundaries.html
  if (NeedsTrailingSpace())
    result = result + " ";

  // When the CSS first-letter pseudoselector is used, the LayoutText for the
  // first letter is excluded from the accessibility tree, so we need to prepend
  // its text here.
  if (LayoutText* first_letter = GetFirstLetterPseudoLayoutText())
    result = first_letter->GetText().SimplifyWhiteSpace() + result;

  return result;
}

bool NGAbstractInlineTextBox::IsFirst() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return true;
  NGInlineCursor first_fragment;
  first_fragment.MoveTo(*cursor.Current().GetLayoutObject());
  return cursor == first_fragment;
}

bool NGAbstractInlineTextBox::IsLast() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return true;
  NGInlineCursor last_fragment;
  last_fragment.MoveTo(*cursor.Current().GetLayoutObject());
  last_fragment.MoveToLastForSameLayoutObject();
  return cursor == last_fragment;
}

scoped_refptr<AbstractInlineTextBox> NGAbstractInlineTextBox::NextOnLine()
    const {
  NGInlineCursor cursor = GetCursorOnLine();
  if (!cursor)
    return nullptr;
  for (cursor.MoveToNext(); cursor; cursor.MoveToNext()) {
    if (cursor.Current().GetLayoutObject()->IsText())
      return GetOrCreate(cursor);
  }
  return nullptr;
}

scoped_refptr<AbstractInlineTextBox> NGAbstractInlineTextBox::PreviousOnLine()
    const {
  NGInlineCursor cursor = GetCursorOnLine();
  if (!cursor)
    return nullptr;
  for (cursor.MoveToPrevious(); cursor; cursor.MoveToPrevious()) {
    if (cursor.Current().GetLayoutObject()->IsText())
      return GetOrCreate(cursor);
  }
  return nullptr;
}

bool NGAbstractInlineTextBox::IsLineBreak() const {
  const NGInlineCursor& cursor = GetCursor();
  return cursor && cursor.Current().IsLineBreak();
}

}  // namespace blink

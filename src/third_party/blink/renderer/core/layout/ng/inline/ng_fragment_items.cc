// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

#if DCHECK_IS_ON()
void CheckNoItemsAreAssociated(const NGPhysicalBoxFragment& fragment) {
  if (const NGFragmentItems* fragment_items = fragment.Items()) {
    for (const NGFragmentItem& item : fragment_items->Items()) {
      if (item.Type() == NGFragmentItem::kLine)
        continue;
      if (const LayoutObject* layout_object = item.GetLayoutObject())
        DCHECK(!layout_object->FirstInlineFragmentItemIndex());
    }
  }
}

void CheckIsLast(const NGFragmentItem& item) {
  if (const NGPhysicalBoxFragment* fragment = item.BoxFragment()) {
    if (!fragment->IsInline()) {
      DCHECK(!fragment->IsInlineBox());
      DCHECK_EQ(item.IsLastForNode(), !fragment->BreakToken());
    }
  }
}
#endif

}  // namespace

NGFragmentItems::NGFragmentItems(NGFragmentItemsBuilder* builder)
    : text_content_(std::move(builder->text_content_)),
      first_line_text_content_(std::move(builder->first_line_text_content_)),
      size_(builder->items_.size()),
      size_of_earlier_fragments_(0) {
  NGFragmentItemsBuilder::ItemWithOffsetList& source_items = builder->items_;
  for (wtf_size_t i = 0; i < size_; ++i) {
    // Call the move constructor to move without |AddRef|. Items in
    // |NGFragmentItemsBuilder| are not used after |this| was constructed.
    new (&items_[i]) NGFragmentItem(std::move(source_items[i].item));
  }
}

NGFragmentItems::NGFragmentItems(const NGFragmentItems& other)
    : text_content_(other.text_content_),
      first_line_text_content_(other.first_line_text_content_),
      size_(other.size_),
      size_of_earlier_fragments_(other.size_of_earlier_fragments_) {
  for (wtf_size_t i = 0; i < size_; ++i) {
    const auto& other_item = other.items_[i];
    new (&items_[i]) NGFragmentItem(other_item);

    // The |other| object is likely going to be freed after this copy. Detach
    // any |AbstractInlineTextBox|, as they store a pointer to an individual
    // |NGFragmentItem|.
    if (auto* layout_text =
            DynamicTo<LayoutText>(other_item.GetMutableLayoutObject()))
      layout_text->DetachAbstractInlineTextBoxesIfNeeded();
  }
}

NGFragmentItems::~NGFragmentItems() {
  for (unsigned i = 0; i < size_; ++i)
    items_[i].~NGFragmentItem();
}

bool NGFragmentItems::IsSubSpan(const Span& span) const {
  return span.empty() ||
         (span.data() >= ItemsData() && &span.back() < ItemsData() + Size());
}

void NGFragmentItems::FinalizeAfterLayout(
    const Vector<scoped_refptr<const NGLayoutResult>, 1>& results) {
#if DCHECK_IS_ON()
  if (!RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled()) {
    for (const auto& result : results) {
      CheckNoItemsAreAssociated(
          To<NGPhysicalBoxFragment>(result->PhysicalFragment()));
    }
  }
#endif
  struct LastItem {
    const NGFragmentItem* item;
    wtf_size_t fragment_id;
    wtf_size_t item_index;
  };
  HashMap<const LayoutObject*, LastItem> last_items;
  wtf_size_t item_index = 0;
  for (const auto& result : results) {
    const auto& fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());
    const NGFragmentItems* fragment_items = fragment.Items();
    if (UNLIKELY(!fragment_items))
      continue;

    fragment_items->size_of_earlier_fragments_ = item_index;
    const Span items = fragment_items->Items();
    for (const NGFragmentItem& item : items) {
      ++item_index;
      if (item.Type() == NGFragmentItem::kLine) {
        DCHECK_EQ(item.DeltaToNextForSameLayoutObject(), 0u);
        continue;
      }
      LayoutObject* const layout_object = item.GetMutableLayoutObject();
      DCHECK(!layout_object->IsOutOfFlowPositioned());
      DCHECK(layout_object->IsInLayoutNGInlineFormattingContext());

      item.SetDeltaToNextForSameLayoutObject(0);
      const bool use_break_token =
          layout_object->IsFloating() || !layout_object->IsInline();
      if (UNLIKELY(use_break_token)) {
        // Fragments that aren't really on a line, such as floats, will have
        // block break tokens if they continue in a subsequent fragmentainer, so
        // just check that. Floats in particular will continue as regular box
        // fragment children in subsequent fragmentainers, i.e. they will not be
        // fragment items (even if we're in an inline formatting context). So
        // we're not going to find the last fragment by just looking for items.
        DCHECK(item.BoxFragment() && !item.BoxFragment()->IsInlineBox());
        item.SetIsLastForNode(!item.BoxFragment()->BreakToken());
      } else {
        DCHECK(layout_object->IsInline());
        // This will be updated later if following fragments are found.
        item.SetIsLastForNode(true);
      }

      // If this is the first fragment, associate with |layout_object|.
      const auto last_item_result =
          last_items.insert(layout_object, LastItem{&item, 0, item_index});
      const bool is_first = last_item_result.is_new_entry;
      if (is_first) {
        item.SetFragmentId(0);
#if DCHECK_IS_ON()
        if (!RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled())
          DCHECK_EQ(layout_object->FirstInlineFragmentItemIndex(), 0u);
#endif
        layout_object->SetFirstInlineFragmentItemIndex(item_index);
        continue;
      }

      // Update the last item for |layout_object|.
      LastItem* last = &last_item_result.stored_value->value;
      const NGFragmentItem* last_item = last->item;
      DCHECK_EQ(last_item->DeltaToNextForSameLayoutObject(), 0u);
      const wtf_size_t last_index = last->item_index;
      DCHECK_GT(last_index, 0u);
      DCHECK_LT(last_index, fragment_items->EndItemIndex());
      DCHECK_LT(last_index, item_index);
      last_item->SetDeltaToNextForSameLayoutObject(item_index - last_index);
      // Because we found a following fragment, reset |IsLastForNode| for the
      // last item except:
      // a. |IsLastForNode| is computed from break token. The last item already
      //    has the correct value.
      // b. Ellipses for atomic inlines. |IsLastForNode| of the last box item
      //    should be set to ease handling of this edge case.
      if (!use_break_token && !(layout_object->IsBox() && item.IsEllipsis()))
        last_item->SetIsLastForNode(false);
#if DCHECK_IS_ON()
      CheckIsLast(*last_item);
#endif

      // Update this item.
      item.SetFragmentId(++last->fragment_id);
      last->item = &item;
      last->item_index = item_index;
    }
  }
#if DCHECK_IS_ON()
  for (const auto& iter : last_items)
    CheckIsLast(*iter.value.item);
#endif
}

void NGFragmentItems::ClearAssociatedFragments(LayoutObject* container) {
  // Clear by traversing |LayoutObject| tree rather than |NGFragmentItem|
  // because a) we don't need to modify |NGFragmentItem|, and in general the
  // number of |LayoutObject| is less than the number of |NGFragmentItem|.
  for (LayoutObject* child = container->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (UNLIKELY(!child->IsInLayoutNGInlineFormattingContext() ||
                 child->IsOutOfFlowPositioned()))
      continue;
    child->ClearFirstInlineFragmentItemIndex();

    // Children of |LayoutInline| are part of this inline formatting context,
    // but children of other |LayoutObject| (e.g., floats, oof, inline-blocks)
    // are not.
    if (child->IsLayoutInline())
      ClearAssociatedFragments(child);
  }
#if DCHECK_IS_ON()
  if (const auto* box = DynamicTo<LayoutBox>(container)) {
    for (const NGPhysicalBoxFragment& fragment : box->PhysicalFragments())
      CheckNoItemsAreAssociated(fragment);
  }
#endif
}

// static
bool NGFragmentItems::CanReuseAll(NGInlineCursor* cursor) {
  for (; *cursor; cursor->MoveToNext()) {
    const NGFragmentItem& item = *cursor->Current().Item();
    if (!item.CanReuse())
      return false;
  }
  return true;
}

const NGFragmentItem* NGFragmentItems::EndOfReusableItems(
    const NGPhysicalBoxFragment& container) const {
  const NGFragmentItem* last_line_start = &front();
  for (NGInlineCursor cursor(container, *this); cursor;) {
    const NGFragmentItem& item = *cursor.Current();
    if (item.IsDirty())
      return &item;

    // Top-level fragments that are not line box cannot be reused; e.g., oof
    // or list markers.
    if (item.Type() != NGFragmentItem::kLine)
      return &item;

    // If there is a dirty item in the middle of a line, its previous line is
    // not reusable, because the dirty item may affect the previous line to wrap
    // differently.
    NGInlineCursor line = cursor.CursorForDescendants();
    if (!CanReuseAll(&line))
      return last_line_start;

    const NGPhysicalLineBoxFragment& line_box_fragment =
        *item.LineBoxFragment();

    // Abort if the line propagated its descendants to outside of the line.
    // They are propagated through NGLayoutResult, which we don't cache.
    if (line_box_fragment.HasPropagatedDescendants())
      return &item;

    // Abort if we are an empty line-box. We don't have any content, and might
    // resolve the BFC block-offset at the incorrect position.
    if (line_box_fragment.IsEmptyLineBox())
      return &item;

    // TODO(kojii): Running the normal layout code at least once for this
    // child helps reducing the code to setup internal states after the
    // partial. Remove the last fragment if it is the end of the
    // fragmentation to do so, but we should figure out how to setup the
    // states without doing this.
    if (!line_box_fragment.BreakToken())
      return &item;

    last_line_start = &item;
    cursor.MoveToNextSkippingChildren();
  }
  return nullptr;  // all items are reusable.
}

// static
bool NGFragmentItems::TryDirtyFirstLineFor(const LayoutObject& layout_object,
                                           const LayoutBlockFlow& container) {
  DCHECK(layout_object.IsDescendantOf(&container));
  NGInlineCursor cursor(container);
  cursor.MoveTo(layout_object);
  if (!cursor)
    return false;
  DCHECK(cursor.Current().Item());
  DCHECK_EQ(&layout_object, cursor.Current().GetLayoutObject());
  cursor.Current()->SetDirty();
  return true;
}

// static
bool NGFragmentItems::TryDirtyLastLineFor(const LayoutObject& layout_object,
                                          const LayoutBlockFlow& container) {
  DCHECK(layout_object.IsDescendantOf(&container));
  NGInlineCursor cursor(container);
  cursor.MoveTo(layout_object);
  if (!cursor)
    return false;
  cursor.MoveToLastForSameLayoutObject();
  DCHECK(cursor.Current().Item());
  DCHECK_EQ(&layout_object, cursor.Current().GetLayoutObject());
  cursor.Current()->SetDirty();
  return true;
}

// static
void NGFragmentItems::DirtyLinesFromChangedChild(
    const LayoutObject& child,
    const LayoutBlockFlow& container) {
  if (child.IsInLayoutNGInlineFormattingContext() &&
      !child.IsFloatingOrOutOfFlowPositioned()) {
    if (TryDirtyFirstLineFor(child, container))
      return;
  }

  // If |child| is new, or did not generate fragments, mark the fragments for
  // previous |LayoutObject| instead.
  for (const LayoutObject* current = &child;;) {
    if (const LayoutObject* previous = current->PreviousSibling()) {
      while (const auto* layout_inline = DynamicTo<LayoutInline>(previous)) {
        if (const LayoutObject* last_child = layout_inline->LastChild())
          previous = last_child;
        else
          break;
      }
      current = previous;
      if (UNLIKELY(current->IsFloatingOrOutOfFlowPositioned()))
        continue;
      if (current->IsInLayoutNGInlineFormattingContext()) {
        if (TryDirtyLastLineFor(*current, container))
          return;
      }
      continue;
    }

    current = current->Parent();
    if (!current || current->IsLayoutBlockFlow()) {
      DirtyFirstItem(container);
      return;
    }
    DCHECK(current->IsLayoutInline());
    if (current->IsInLayoutNGInlineFormattingContext()) {
      if (TryDirtyFirstLineFor(*current, container))
        return;
    }
  }
}

// static
void NGFragmentItems::DirtyFirstItem(const LayoutBlockFlow& container) {
  for (const NGPhysicalBoxFragment& fragment : container.PhysicalFragments()) {
    if (const NGFragmentItems* items = fragment.Items()) {
      items->front().SetDirty();
      return;
    }
  }
}

// static
void NGFragmentItems::DirtyLinesFromNeedsLayout(
    const LayoutBlockFlow& container) {
  DCHECK(std::any_of(container.PhysicalFragments().begin(),
                     container.PhysicalFragments().end(),
                     [](const NGPhysicalBoxFragment& fragment) {
                       return fragment.HasItems();
                     }));

  // Mark dirty for the first top-level child that has |NeedsLayout|.
  //
  // TODO(kojii): We could mark first descendant to increase reuse
  // opportunities. Doing this complicates the logic, especially when culled
  // inline is involved, and common case is to append to large IFC. Choose
  // simpler logic and faster to check over more reuse opportunities.
  const auto writing_mode = container.StyleRef().GetWritingMode();
  for (LayoutObject* child = container.FirstChild(); child;
       child = child->NextSibling()) {
    // NeedsLayout is not helpful for an orthogonal writing-mode root because
    // its NeedsLayout flag is cleared during the ComputeMinMaxSizes() step of
    // the container.
    if (child->NeedsLayout() ||
        !IsParallelWritingMode(writing_mode,
                               child->StyleRef().GetWritingMode())) {
      DirtyLinesFromChangedChild(*child, container);
      return;
    }
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeMoved(
    const LayoutObject& layout_object) {
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeMoved();
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeDestroyed(
    const LayoutObject& layout_object) {
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeDestroyed();
  }
}

#if DCHECK_IS_ON()
void NGFragmentItems::CheckAllItemsAreValid() const {
  for (const NGFragmentItem& item : Items())
    DCHECK(!item.IsLayoutObjectDestroyedOrMoved());
}
#endif

}  // namespace blink

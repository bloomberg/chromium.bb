/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutListItem.h"

#include "core/HTMLNames.h"
#include "core/dom/FlatTreeTraversal.h"
#include "core/html/HTMLOListElement.h"
#include "core/layout/LayoutListMarker.h"
#include "core/paint/ListItemPainter.h"
#include "platform/wtf/SaturatedArithmetic.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

LayoutListItem::LayoutListItem(Element* element)
    : LayoutBlockFlow(element),
      marker_(nullptr),
      has_explicit_value_(false),
      is_value_up_to_date_(false),
      not_in_list_(false) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
}

void LayoutListItem::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  LayoutBlockFlow::StyleDidChange(diff, old_style);

  StyleImage* current_image = Style()->ListStyleImage();
  if (Style()->ListStyleType() != EListStyleType::kNone ||
      (current_image && !current_image->ErrorOccurred())) {
    if (!marker_)
      marker_ = LayoutListMarker::CreateAnonymous(this);
    marker_->ListItemStyleDidChange();
    NotifyOfSubtreeChange();
  } else if (marker_) {
    marker_->Destroy();
    marker_ = nullptr;
  }

  StyleImage* old_image = old_style ? old_style->ListStyleImage() : nullptr;
  if (old_image != current_image) {
    if (old_image)
      old_image->RemoveClient(this);
    if (current_image)
      current_image->AddClient(this);
  }
}

void LayoutListItem::WillBeDestroyed() {
  if (marker_) {
    marker_->Destroy();
    marker_ = nullptr;
  }

  LayoutBlockFlow::WillBeDestroyed();

  if (Style() && Style()->ListStyleImage())
    Style()->ListStyleImage()->RemoveClient(this);
}

void LayoutListItem::InsertedIntoTree() {
  LayoutBlockFlow::InsertedIntoTree();

  UpdateListMarkerNumbers();
}

void LayoutListItem::WillBeRemovedFromTree() {
  LayoutBlockFlow::WillBeRemovedFromTree();

  UpdateListMarkerNumbers();
}

void LayoutListItem::SubtreeDidChange() {
  if (!marker_)
    return;

  if (!UpdateMarkerLocation())
    return;

  // If the marker is inside we need to redo the preferred width calculations
  // as the size of the item now includes the size of the list marker.
  if (marker_->IsInside())
    SetPreferredLogicalWidthsDirty();
}

static bool IsList(const Node& node) {
  return isHTMLUListElement(node) || isHTMLOListElement(node);
}

// Returns the enclosing list with respect to the DOM order.
static Node* EnclosingList(const LayoutListItem* list_item) {
  Node* list_item_node = list_item->GetNode();
  if (!list_item_node)
    return nullptr;
  Node* first_node = nullptr;
  // We use parentNode because the enclosing list could be a ShadowRoot that's
  // not Element.
  for (Node* parent = FlatTreeTraversal::Parent(*list_item_node); parent;
       parent = FlatTreeTraversal::Parent(*parent)) {
    if (IsList(*parent))
      return parent;
    if (!first_node)
      first_node = parent;
  }

  // If there's no actual <ul> or <ol> list element, then the first found
  // node acts as our list for purposes of determining what other list items
  // should be numbered as part of the same list.
  return first_node;
}

// Returns the next list item with respect to the DOM order.
static LayoutListItem* NextListItem(const Node* list_node,
                                    const LayoutListItem* item = nullptr) {
  if (!list_node)
    return nullptr;

  const Node* current = item ? item->GetNode() : list_node;
  DCHECK(current);
  DCHECK(!current->GetDocument().ChildNeedsDistributionRecalc());
  current = LayoutTreeBuilderTraversal::Next(*current, list_node);

  while (current) {
    if (IsList(*current)) {
      // We've found a nested, independent list: nothing to do here.
      current =
          LayoutTreeBuilderTraversal::NextSkippingChildren(*current, list_node);
      continue;
    }

    LayoutObject* layout_object = current->GetLayoutObject();
    if (layout_object && layout_object->IsListItem())
      return ToLayoutListItem(layout_object);

    // FIXME: Can this be optimized to skip the children of the elements without
    // a layoutObject?
    current = LayoutTreeBuilderTraversal::Next(*current, list_node);
  }

  return nullptr;
}

// Returns the previous list item with respect to the DOM order.
static LayoutListItem* PreviousListItem(const Node* list_node,
                                        const LayoutListItem* item) {
  Node* current = item->GetNode();
  DCHECK(current);
  DCHECK(!current->GetDocument().ChildNeedsDistributionRecalc());
  for (current = LayoutTreeBuilderTraversal::Previous(*current, list_node);
       current && current != list_node;
       current = LayoutTreeBuilderTraversal::Previous(*current, list_node)) {
    LayoutObject* layout_object = current->GetLayoutObject();
    if (!layout_object || (layout_object && !layout_object->IsListItem()))
      continue;
    Node* other_list = EnclosingList(ToLayoutListItem(layout_object));
    // This item is part of our current list, so it's what we're looking for.
    if (list_node == other_list)
      return ToLayoutListItem(layout_object);
    // We found ourself inside another list; lets skip the rest of it.
    // Use nextIncludingPseudo() here because the other list itself may actually
    // be a list item itself. We need to examine it, so we do this to counteract
    // the previousIncludingPseudo() that will be done by the loop.
    if (other_list)
      current = LayoutTreeBuilderTraversal::Next(*other_list, list_node);
  }
  return nullptr;
}

void LayoutListItem::UpdateItemValuesForOrderedList(
    const HTMLOListElement* list_node) {
  DCHECK(list_node);

  for (LayoutListItem* list_item = NextListItem(list_node); list_item;
       list_item = NextListItem(list_node, list_item))
    list_item->UpdateValue();
}

unsigned LayoutListItem::ItemCountForOrderedList(
    const HTMLOListElement* list_node) {
  DCHECK(list_node);

  unsigned item_count = 0;
  for (LayoutListItem* list_item = NextListItem(list_node); list_item;
       list_item = NextListItem(list_node, list_item))
    item_count++;

  return item_count;
}

inline int LayoutListItem::CalcValue() const {
  if (has_explicit_value_)
    return explicit_value_;

  Node* list = EnclosingList(this);
  HTMLOListElement* o_list_element =
      isHTMLOListElement(list) ? toHTMLOListElement(list) : nullptr;
  int value_step = 1;
  if (o_list_element && o_list_element->IsReversed())
    value_step = -1;

  // FIXME: This recurses to a possible depth of the length of the list.
  // That's not good -- we need to change this to an iterative algorithm.
  if (LayoutListItem* previous_item = PreviousListItem(list, this))
    return ClampAdd(previous_item->Value(), value_step);

  if (o_list_element)
    return o_list_element->start();

  return 1;
}

void LayoutListItem::UpdateValueNow() const {
  value_ = CalcValue();
  is_value_up_to_date_ = true;
}

bool LayoutListItem::IsEmpty() const {
  return LastChild() == marker_;
}

static LayoutObject* GetParentOfFirstLineBox(LayoutBlockFlow* curr,
                                             LayoutObject* marker) {
  LayoutObject* first_child = curr->FirstChild();
  if (!first_child)
    return nullptr;

  bool in_quirks_mode = curr->GetDocument().InQuirksMode();
  for (LayoutObject* curr_child = first_child; curr_child;
       curr_child = curr_child->NextSibling()) {
    if (curr_child == marker)
      continue;

    // Shouldn't add marker into Overflow box, instead, add marker
    // into listitem
    if (curr->HasOverflowClip())
      break;

    if (curr_child->IsInline() &&
        (!curr_child->IsLayoutInline() ||
         curr->GeneratesLineBoxesForInlineChild(curr_child)))
      return curr;

    if (curr_child->IsFloating() || curr_child->IsOutOfFlowPositioned())
      continue;

    if (!curr_child->IsLayoutBlockFlow() ||
        (curr_child->IsBox() && ToLayoutBox(curr_child)->IsWritingModeRoot()))
      break;

    if (curr->IsListItem() && in_quirks_mode && curr_child->GetNode() &&
        (isHTMLUListElement(*curr_child->GetNode()) ||
         isHTMLOListElement(*curr_child->GetNode())))
      break;

    LayoutObject* line_box =
        GetParentOfFirstLineBox(ToLayoutBlockFlow(curr_child), marker);
    if (line_box)
      return line_box;
  }

  return nullptr;
}

void LayoutListItem::UpdateValue() {
  if (!has_explicit_value_) {
    is_value_up_to_date_ = false;
    if (marker_)
      marker_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::kListValueChange);
  }
}

static LayoutObject* FirstNonMarkerChild(LayoutObject* parent) {
  LayoutObject* result = parent->SlowFirstChild();
  while (result && result->IsListMarker())
    result = result->NextSibling();
  return result;
}

bool LayoutListItem::UpdateMarkerLocation() {
  DCHECK(marker_);

  LayoutObject* marker_parent = marker_->Parent();
  // list-style-position:inside makes the ::marker pseudo an ordinary
  // position:static element that should be attached to LayoutListItem block.
  LayoutObject* line_box_parent =
      marker_->IsInside() ? this : GetParentOfFirstLineBox(this, marker_);
  if (!line_box_parent) {
    // If the marker is currently contained inside an anonymous box, then we
    // are the only item in that anonymous box (since no line box parent was
    // found). It's ok to just leave the marker where it is in this case.
    if (marker_parent && marker_parent->IsAnonymousBlock())
      line_box_parent = marker_parent;
    else
      line_box_parent = this;
  }

  if (marker_parent != line_box_parent) {
    marker_->Remove();
    line_box_parent->AddChild(marker_, FirstNonMarkerChild(line_box_parent));
    // TODO(rhogan): lineBoxParent and markerParent may be deleted by addChild,
    // so they are not safe to reference here.
    // Once we have a safe way of referencing them delete markerParent if it is
    // an empty anonymous block.
    marker_->UpdateMarginsAndContent();
    return true;
  }

  return false;
}

void LayoutListItem::AddOverflowFromChildren() {
  LayoutBlockFlow::AddOverflowFromChildren();
  PositionListMarker();
}

void LayoutListItem::PositionListMarker() {
  if (marker_ && marker_->Parent() && marker_->Parent()->IsBox() &&
      !marker_->IsInside() && marker_->InlineBoxWrapper()) {
    LayoutUnit marker_old_logical_left = marker_->LogicalLeft();
    LayoutUnit block_offset;
    LayoutUnit line_offset;
    for (LayoutBox* o = marker_->ParentBox(); o != this; o = o->ParentBox()) {
      block_offset += o->LogicalTop();
      line_offset += o->LogicalLeft();
    }

    bool adjust_overflow = false;
    LayoutUnit marker_logical_left;
    RootInlineBox& root = marker_->InlineBoxWrapper()->Root();
    bool hit_self_painting_layer = false;

    LayoutUnit line_top = root.LineTop();
    LayoutUnit line_bottom = root.LineBottom();

    // TODO(jchaffraix): Propagating the overflow to the line boxes seems
    // pretty wrong (https://crbug.com/554160).
    // FIXME: Need to account for relative positioning in the layout overflow.
    if (Style()->IsLeftToRightDirection()) {
      marker_logical_left = marker_->LineOffset() - line_offset -
                            PaddingStart() - BorderStart() +
                            marker_->MarginStart();
      marker_->InlineBoxWrapper()->MoveInInlineDirection(
          marker_logical_left - marker_old_logical_left);
      for (InlineFlowBox* box = marker_->InlineBoxWrapper()->Parent(); box;
           box = box->Parent()) {
        LayoutRect new_logical_visual_overflow_rect =
            box->LogicalVisualOverflowRect(line_top, line_bottom);
        LayoutRect new_logical_layout_overflow_rect =
            box->LogicalLayoutOverflowRect(line_top, line_bottom);
        if (marker_logical_left < new_logical_visual_overflow_rect.X() &&
            !hit_self_painting_layer) {
          new_logical_visual_overflow_rect.SetWidth(
              new_logical_visual_overflow_rect.MaxX() - marker_logical_left);
          new_logical_visual_overflow_rect.SetX(marker_logical_left);
          if (box == root)
            adjust_overflow = true;
        }
        if (marker_logical_left < new_logical_layout_overflow_rect.X()) {
          new_logical_layout_overflow_rect.SetWidth(
              new_logical_layout_overflow_rect.MaxX() - marker_logical_left);
          new_logical_layout_overflow_rect.SetX(marker_logical_left);
          if (box == root)
            adjust_overflow = true;
        }
        box->OverrideOverflowFromLogicalRects(new_logical_layout_overflow_rect,
                                              new_logical_visual_overflow_rect,
                                              line_top, line_bottom);
        if (box->BoxModelObject().HasSelfPaintingLayer())
          hit_self_painting_layer = true;
      }
    } else {
      marker_logical_left = marker_->LineOffset() - line_offset +
                            PaddingStart() + BorderStart() +
                            marker_->MarginEnd();
      marker_->InlineBoxWrapper()->MoveInInlineDirection(
          marker_logical_left - marker_old_logical_left);
      for (InlineFlowBox* box = marker_->InlineBoxWrapper()->Parent(); box;
           box = box->Parent()) {
        LayoutRect new_logical_visual_overflow_rect =
            box->LogicalVisualOverflowRect(line_top, line_bottom);
        LayoutRect new_logical_layout_overflow_rect =
            box->LogicalLayoutOverflowRect(line_top, line_bottom);
        if (marker_logical_left + marker_->LogicalWidth() >
                new_logical_visual_overflow_rect.MaxX() &&
            !hit_self_painting_layer) {
          new_logical_visual_overflow_rect.SetWidth(
              marker_logical_left + marker_->LogicalWidth() -
              new_logical_visual_overflow_rect.X());
          if (box == root)
            adjust_overflow = true;
        }
        if (marker_logical_left + marker_->LogicalWidth() >
            new_logical_layout_overflow_rect.MaxX()) {
          new_logical_layout_overflow_rect.SetWidth(
              marker_logical_left + marker_->LogicalWidth() -
              new_logical_layout_overflow_rect.X());
          if (box == root)
            adjust_overflow = true;
        }
        box->OverrideOverflowFromLogicalRects(new_logical_layout_overflow_rect,
                                              new_logical_visual_overflow_rect,
                                              line_top, line_bottom);

        if (box->BoxModelObject().HasSelfPaintingLayer())
          hit_self_painting_layer = true;
      }
    }

    if (adjust_overflow) {
      LayoutRect marker_rect(
          LayoutPoint(marker_logical_left + line_offset, block_offset),
          marker_->Size());
      if (!Style()->IsHorizontalWritingMode())
        marker_rect = marker_rect.TransposedRect();
      LayoutBox* o = marker_;
      bool propagate_visual_overflow = true;
      bool propagate_layout_overflow = true;
      do {
        o = o->ParentBox();
        if (o->IsLayoutBlock()) {
          if (propagate_visual_overflow)
            ToLayoutBlock(o)->AddContentsVisualOverflow(marker_rect);
          if (propagate_layout_overflow)
            ToLayoutBlock(o)->AddLayoutOverflow(marker_rect);
        }
        if (o->HasOverflowClip()) {
          propagate_layout_overflow = false;
          propagate_visual_overflow = false;
        }
        if (o->HasSelfPaintingLayer())
          propagate_visual_overflow = false;
        marker_rect.MoveBy(-o->Location());
      } while (o != this && propagate_visual_overflow &&
               propagate_layout_overflow);
    }
  }
}

void LayoutListItem::Paint(const PaintInfo& paint_info,
                           const LayoutPoint& paint_offset) const {
  ListItemPainter(*this).Paint(paint_info, paint_offset);
}

const String& LayoutListItem::MarkerText() const {
  if (marker_)
    return marker_->GetText();
  return g_null_atom.GetString();
}

void LayoutListItem::ExplicitValueChanged() {
  if (marker_)
    marker_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::kListValueChange);
  Node* list_node = EnclosingList(this);
  for (LayoutListItem* item = this; item; item = NextListItem(list_node, item))
    item->UpdateValue();
}

void LayoutListItem::SetExplicitValue(int value) {
  DCHECK(GetNode());

  if (has_explicit_value_ && explicit_value_ == value)
    return;
  explicit_value_ = value;
  value_ = value;
  has_explicit_value_ = true;
  ExplicitValueChanged();
}

void LayoutListItem::ClearExplicitValue() {
  DCHECK(GetNode());

  if (!has_explicit_value_)
    return;
  has_explicit_value_ = false;
  is_value_up_to_date_ = false;
  ExplicitValueChanged();
}

void LayoutListItem::SetNotInList(bool not_in_list) {
  not_in_list_ = not_in_list;
}

static LayoutListItem* PreviousOrNextItem(bool is_list_reversed,
                                          Node* list,
                                          LayoutListItem* item) {
  return is_list_reversed ? PreviousListItem(list, item)
                          : NextListItem(list, item);
}

void LayoutListItem::UpdateListMarkerNumbers() {
  // If distribution recalc is needed, updateListMarkerNumber will be re-invoked
  // after distribution is calculated.
  if (GetNode()->GetDocument().ChildNeedsDistributionRecalc())
    return;

  Node* list_node = EnclosingList(this);
  CHECK(list_node);

  bool is_list_reversed = false;
  HTMLOListElement* o_list_element =
      isHTMLOListElement(list_node) ? toHTMLOListElement(list_node) : 0;
  if (o_list_element) {
    o_list_element->ItemCountChanged();
    is_list_reversed = o_list_element->IsReversed();
  }

  // FIXME: The n^2 protection below doesn't help if the elements were inserted
  // after the the list had already been displayed.

  // Avoid an O(n^2) walk over the children below when they're all known to be
  // attaching.
  if (list_node->NeedsAttach())
    return;

  for (LayoutListItem* item =
           PreviousOrNextItem(is_list_reversed, list_node, this);
       item; item = PreviousOrNextItem(is_list_reversed, list_node, item)) {
    if (!item->is_value_up_to_date_) {
      // If an item has been marked for update before, we can safely
      // assume that all the following ones have too.
      // This gives us the opportunity to stop here and avoid
      // marking the same nodes again.
      break;
    }
    item->UpdateValue();
  }
}

}  // namespace blink

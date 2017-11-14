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

#include "core/dom/FlatTreeTraversal.h"
#include "core/html/HTMLLIElement.h"
#include "core/html/HTMLOListElement.h"
#include "core/html_names.h"
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
      need_block_direction_align_(false) {
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

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutListItem::WillBeRemovedFromTree() {
  LayoutBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
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

int LayoutListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
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

    if (curr_child->IsInline() &&
        (!curr_child->IsLayoutInline() ||
         curr->GeneratesLineBoxesForInlineChild(curr_child)))
      return curr;

    if (curr_child->IsFloating() || curr_child->IsOutOfFlowPositioned())
      continue;

    if (curr->HasOverflowClip())
      return curr;

    if (!curr_child->IsLayoutBlockFlow() ||
        (curr_child->IsBox() && ToLayoutBox(curr_child)->IsWritingModeRoot()))
      return curr_child;

    if (curr->IsListItem() && in_quirks_mode && curr_child->GetNode() &&
        (IsHTMLUListElement(*curr_child->GetNode()) ||
         IsHTMLOListElement(*curr_child->GetNode())))
      break;

    LayoutObject* line_box =
        GetParentOfFirstLineBox(ToLayoutBlockFlow(curr_child), marker);
    if (line_box)
      return line_box;
  }

  return nullptr;
}

static LayoutObject* FirstNonMarkerChild(LayoutObject* parent) {
  LayoutObject* result = parent->SlowFirstChild();
  while (result && result->IsListMarker())
    result = result->NextSibling();
  return result;
}

// 1. Place marker as a child of <li>. Make sure don't share parent with empty
// inline elements which don't gernerate inlineBox.
// 2. Manage the logicalHeight of marker_container(marker's anonymous parent):
// If marker is the only child of marker_container, set logicalHeight of
// marker_container to 0px; else restore it to logicalHeight of <li>.
bool LayoutListItem::PrepareForBlockDirectionAlign(
    const LayoutObject* line_box_parent) {
  LayoutObject* marker_parent = marker_->Parent();
  // Deal with the situation of layout tree changed.
  if (marker_parent && marker_parent->IsAnonymous()) {
    // When list-position-style change from outside to inside, we need to
    // restore logicalHeight. So add isinside().
    if (marker_->IsInside() || marker_->NextSibling()) {
      // Restore old marker_container logicalHeight.
      if (marker_parent->MutableStyleRef().LogicalHeight().IsZero()) {
        marker_parent->MutableStyleRef().SetLogicalHeight(
            Style()->LogicalHeight());
      }

      // If marker_parent isn't the ancestor of line_box_parent, marker might
      // generate a new empty line. We need to remove marker here.E.g:
      // <li><span><div>text<div><span></li>
      if (line_box_parent && !line_box_parent->IsDescendantOf(marker_parent)) {
        marker_->Remove();
        marker_parent = nullptr;
      }
    } else {
      if (line_box_parent)
        marker_parent->MutableStyleRef().SetLogicalHeight(Length(0, kFixed));
    }
  }

  // Create marker_container, set its height to 0px, and add it to li.
  if (!marker_parent) {
    LayoutObject* before_child = FirstNonMarkerChild(this);
    if (!marker_->IsInside() && before_child && before_child->IsLayoutBlock()) {
      // Create marker_container and set its logicalHeight to 0px.
      LayoutBlock* marker_container = CreateAnonymousBlock();
      if (line_box_parent)
        marker_container->MutableStyleRef().SetLogicalHeight(Length(0, kFixed));
      marker_container->AddChild(marker_,
                                 FirstNonMarkerChild(marker_container));
      this->AddChild(marker_container, before_child);
    } else {
      this->AddChild(marker_, before_child);
    }

    marker_->UpdateMarginsAndContent();
    return true;
  }
  return false;
}

bool LayoutListItem::UpdateMarkerLocation() {
  DCHECK(marker_);

  LayoutObject* marker_parent = marker_->Parent();
  // list-style-position:inside makes the ::marker pseudo an ordinary
  // position:static element that should be attached to LayoutListItem block.
  LayoutObject* line_box_parent =
      marker_->IsInside() ? this : GetParentOfFirstLineBox(this, marker_);

  if (!marker_->IsInside() && line_box_parent &&
      (line_box_parent->HasOverflowClip() ||
       !line_box_parent->IsLayoutBlockFlow() ||
       (line_box_parent->IsBox() &&
        ToLayoutBox(line_box_parent)->IsWritingModeRoot())))
    need_block_direction_align_ = true;
  if (need_block_direction_align_)
    return PrepareForBlockDirectionAlign(line_box_parent);

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

// Align marker_inline_box in block direction according to line_box_root's
// baseline.
void LayoutListItem::AlignMarkerInBlockDirection() {
  LayoutObject* line_box_parent = GetParentOfFirstLineBox(this, marker_);
  if (!line_box_parent || !line_box_parent->IsBox())
    return;

  LayoutBox* line_box_parent_block = ToLayoutBox(line_box_parent);
  // Don't align marker if line_box_parent has a different writing-mode.
  // Just let marker positioned at the left-top of line_box_parent.
  if (line_box_parent_block->IsWritingModeRoot())
    return;

  InlineBox* marker_inline_box = marker_->InlineBoxWrapper();
  RootInlineBox& marker_root = marker_inline_box->Root();
  if (line_box_parent_block->IsLayoutBlockFlow()) {
    // If marker_ and line_box_parent_block share a same RootInlineBox, no need
    // to align marker.
    if (ToLayoutBlockFlow(line_box_parent_block)->FirstRootBox() ==
        &marker_root)
      return;
  }

  LayoutUnit offset = line_box_parent_block->FirstLineBoxBaseline();
  if (offset != -1) {
    for (LayoutBox* o = line_box_parent_block; o != this; o = o->ParentBox())
      offset += o->LogicalTop();

    // Compute marker_inline_box's baseline.
    // We shouldn't use FirstLineBoxBaseline here. FirstLineBoxBaseline is
    // the baseline of root. We should compute marker_inline_box's baseline
    // instead. BaselinePosition is workable when marker is an image.
    // However, when marker is text, BaselinePosition contains lineheight
    // information. So use marker_font_metrics.Ascent when marker is text.
    if (marker_->IsImage()) {
      offset -= marker_inline_box->BaselinePosition(marker_root.BaselineType());
    } else {
      const SimpleFontData* marker_font_data =
          marker_->Style(true)->GetFont().PrimaryFont();
      if (marker_font_data) {
        const FontMetrics& marker_font_metrics =
            marker_font_data->GetFontMetrics();
        offset -= marker_font_metrics.Ascent(marker_root.BaselineType());
      }
    }
    offset -= marker_inline_box->LogicalTop();

    for (LayoutBox* o = marker_->ParentBox(); o != this; o = o->ParentBox()) {
      offset -= o->LogicalTop();
    }

    marker_inline_box->MoveInBlockDirection(offset);
  }
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

    if (need_block_direction_align_)
      AlignMarkerInBlockDirection();

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

void LayoutListItem::OrdinalValueChanged() {
  if (!marker_)
    return;
  marker_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kListValueChange);
}

}  // namespace blink

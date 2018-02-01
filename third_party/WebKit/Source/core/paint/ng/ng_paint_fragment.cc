// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_paint_fragment.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_container_fragment.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/paint/ng/ng_box_fragment_painter.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

NGPaintFragment::NGPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPaintFragment* parent)
    : physical_fragment_(std::move(fragment)), parent_(parent) {
  DCHECK(physical_fragment_);
}

std::unique_ptr<NGPaintFragment> NGPaintFragment::Create(
    scoped_refptr<const NGPhysicalFragment> fragment) {
  std::unique_ptr<NGPaintFragment> paint_fragment =
      std::make_unique<NGPaintFragment>(std::move(fragment), nullptr);

  HashMap<const LayoutObject*, NGPaintFragment*> last_fragment_map;
  paint_fragment->PopulateDescendants(NGPhysicalOffset(),
                                      &paint_fragment->first_fragment_map_,
                                      &last_fragment_map);

  return paint_fragment;
}

bool NGPaintFragment::HasSelfPaintingLayer() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).HasSelfPaintingLayer();
}

bool NGPaintFragment::HasOverflowClip() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).HasOverflowClip();
}

bool NGPaintFragment::ShouldClipOverflow() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).ShouldClipOverflow();
}

LayoutRect NGPaintFragment::VisualOverflowRect() const {
  return physical_fragment_->VisualRectWithContents().ToLayoutRect();
}

// Populate descendants from NGPhysicalFragment tree.
void NGPaintFragment::PopulateDescendants(
    const NGPhysicalOffset inline_offset_to_container_box,
    HashMap<const LayoutObject*, NGPaintFragment*>* first_fragment_map,
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map) {
  DCHECK(children_.IsEmpty());
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (!fragment.IsContainer())
    return;
  const NGPhysicalContainerFragment& container =
      ToNGPhysicalContainerFragment(fragment);
  children_.ReserveCapacity(container.Children().size());

  for (const auto& child_fragment : container.Children()) {
    std::unique_ptr<NGPaintFragment> child =
        std::make_unique<NGPaintFragment>(child_fragment, this);

    // Create a linked list for each LayoutObject.
    //
    // |first_fragment_map| and |last_fragment_map| each keeps the first and the
    // last of the list of fragments for a LayoutObject. We use two maps because
    // |last_fragment_map| is needed only while creating lists, while
    // |first_fragment_map| is kept for later queries. This may change when we
    // use fields in LayoutObject to keep the first fragments.
    if (LayoutObject* layout_object = child_fragment->GetLayoutObject()) {
      auto add_result = last_fragment_map->insert(layout_object, child.get());
      if (add_result.is_new_entry) {
        DCHECK(first_fragment_map->find(layout_object) ==
               first_fragment_map->end());
        first_fragment_map->insert(layout_object, child.get());
      } else {
        DCHECK(first_fragment_map->find(layout_object) !=
               first_fragment_map->end());
        DCHECK(add_result.stored_value->value);
        add_result.stored_value->value->next_fragment_ = child.get();
        add_result.stored_value->value = child.get();
      }
    }

    child->inline_offset_to_container_box_ =
        inline_offset_to_container_box + child_fragment->Offset();

    // Recurse chlidren, except when this is a block layout root.
    // TODO(kojii): At the block layout root, chlidren maybe for NGPaint,
    // LayoutNG but not for NGPaint, or legacy. In order to get the maximum
    // test coverage, split the NGPaintFragment tree at all possible engine
    // boundaries.
    if (!child_fragment->IsBlockLayoutRoot()) {
      child->PopulateDescendants(child->inline_offset_to_container_box_,
                                 first_fragment_map, last_fragment_map);
    }

    children_.push_back(std::move(child));
  }
}

NGPaintFragment::FragmentRange NGPaintFragment::InlineFragmentsFor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline());
  if (LayoutBlockFlow* block_flow = layout_object->EnclosingNGBlockFlow()) {
    if (const NGPaintFragment* root = block_flow->PaintFragment()) {
      auto it = root->first_fragment_map_.find(layout_object);
      if (it != root->first_fragment_map_.end())
        return FragmentRange(it->value);

      // TODO(kojii): This is a culled inline box. Should we handle the case
      // here, or by the caller, or even stop the culled inline boxes?
      DCHECK(layout_object->IsLayoutInline());
    }
  }
  return FragmentRange(nullptr);
}

// TODO(kojii): This code copies VisualRects from the LayoutObject tree. We
// should implement the pre-paint tree walk on the NGPaintFragment tree, and
// save this work.
void NGPaintFragment::UpdateVisualRectFromLayoutObject() {
  DCHECK_EQ(PhysicalFragment().Type(), NGPhysicalFragment::kFragmentBox);

  UpdateVisualRectFromLayoutObject({});
}

void NGPaintFragment::UpdateVisualRectFromLayoutObject(
    const NGPhysicalOffset& parent_paint_offset) {
  // Compute VisualRect from fragment if:
  // - Text fragment, including generated content
  // - Line box fragment (does not have LayoutObject)
  // - Box fragment for inline box
  // VisualRect() of LayoutText/LayoutInline is the union of corresponding
  // fragments.
  const NGPhysicalFragment& fragment = PhysicalFragment();
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  bool is_inline =
      fragment.IsText() || fragment.IsLineBox() ||
      (fragment.IsBox() && layout_object && layout_object->IsLayoutInline());
  NGPhysicalOffset paint_offset;
  if (is_inline) {
    NGPhysicalOffsetRect visual_rect = fragment.SelfVisualRect();
    paint_offset = parent_paint_offset + fragment.Offset();
    visual_rect.offset += paint_offset;
    SetVisualRect(visual_rect.ToLayoutRect());
  } else {
    // Copy the VisualRect from the corresponding LayoutObject.
    // PaintInvalidator has set the correct VisualRect to LayoutObject, computed
    // from SelfVisualRect().
    // TODO(kojii): The relationship of fragment_data and NG multicol isn't
    // clear yet. For now, this copies from the union of fragment visual rects.
    // This should be revisited if this code lives long, but the hope is for
    // PaintInvalidator to walk NG fragment tree is not that far.
    DCHECK(layout_object && layout_object->IsBox());
    DCHECK_EQ(fragment.Type(), NGPhysicalFragment::kFragmentBox);
    const DisplayItemClient* client = layout_object;
    SetVisualRect(client->VisualRect());
  }

  if (!children_.IsEmpty()) {
    // |VisualRect| of children of an inline box are relative to their inline
    // formatting context. Accumulate offset to convert to the |VisualRect|
    // space.
    if (!is_inline) {
      DCHECK(layout_object);
      paint_offset =
          NGPhysicalOffset{layout_object->FirstFragment().PaintOffset()};
    }
    for (auto& child : children_) {
      child->UpdateVisualRectFromLayoutObject(paint_offset);
    }
  }
}

void NGPaintFragment::AddSelfOutlineRect(
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset) const {
  DCHECK(outline_rects);
  //
  LayoutRect outline_rect(additional_offset, Size().ToLayoutSize());
  // LayoutRect outline_rect = VisualRect();
  // outline_rect.MoveBy(additional_offset);
  // outline_rect.Inflate(-Style().OutlineOffset());
  // outline_rect.Inflate(-Style().OutlineWidth());

  outline_rects->push_back(outline_rect);
}

void NGPaintFragment::PaintInlineBoxForDescendants(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutInline* layout_object,
    NGPhysicalOffset offset) const {
  for (const auto& child : Children()) {
    if (child->GetLayoutObject() == layout_object) {
      NGBoxFragmentPainter(*child).PaintInlineBox(
          paint_info, paint_offset + offset.ToLayoutPoint() /*, paint_offset*/);
      continue;
    }

    child->PaintInlineBoxForDescendants(paint_info, paint_offset, layout_object,
                                        offset + child->Offset());
  }
}

}  // namespace blink

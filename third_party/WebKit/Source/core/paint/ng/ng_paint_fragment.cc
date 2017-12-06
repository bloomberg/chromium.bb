// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_paint_fragment.h"

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
    bool stop_at_block_layout_root)
    : physical_fragment_(std::move(fragment)) {
  DCHECK(physical_fragment_);
  PopulateDescendants(stop_at_block_layout_root);
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
//
// |stop_at_block_layout_root| is set to |false| on the root fragment because
// most root fragments are block layout root but their children are needed.
// For children, it is set to |true| to stop at the block layout root.
void NGPaintFragment::PopulateDescendants(bool stop_at_block_layout_root) {
  DCHECK(children_.IsEmpty());
  const NGPhysicalFragment& fragment = PhysicalFragment();
  // Recurse chlidren, except when this is a block layout root.
  if (fragment.IsContainer() &&
      !(fragment.IsBlockLayoutRoot() && stop_at_block_layout_root)) {
    const NGPhysicalContainerFragment& container =
        ToNGPhysicalContainerFragment(fragment);
    children_.ReserveCapacity(container.Children().size());
    for (const auto& child_fragment : container.Children()) {
      children_.push_back(
          std::make_unique<NGPaintFragment>(child_fragment, true));
    }
  }
}

// TODO(kojii): This code copies VisualRects from the LayoutObject tree. We
// should implement the pre-paint tree walk on the NGPaintFragment tree, and
// save this work.
void NGPaintFragment::UpdateVisualRectFromLayoutObject() {
  DCHECK_EQ(PhysicalFragment().Type(), NGPhysicalFragment::kFragmentBox);

  UpdateVisualRectFromLayoutObject({nullptr});
}

void NGPaintFragment::UpdateVisualRectFromLayoutObject(
    const UpdateContext& context) {
  // Compute VisualRect from fragment if:
  // - Text fragment, including generated content
  // - Line box fragment (does not have LayoutObject)
  // - Box fragment for inline box
  // VisualRect() of LayoutText/LayoutInline is the union of corresponding
  // fragments.
  const NGPhysicalFragment& fragment = PhysicalFragment();
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  if (fragment.IsText() || fragment.IsLineBox() ||
      (fragment.IsBox() && layout_object && layout_object->IsLayoutInline())) {
    NGPhysicalOffsetRect visual_rect = fragment.SelfVisualRect();
    DCHECK(context.parent_box);
    // TODO(kojii): Review the use of FirstFragment() and PaintOffset(). This is
    // likely incorrect.
    visual_rect.offset +=
        fragment.Offset() + context.offset_to_parent_box +
        NGPhysicalOffset(context.parent_box->FirstFragment().PaintOffset());
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
    // If this fragment isn't from a LayoutObject; i.e., a line box or an
    // anonymous, keep the offset to the parent box.
    UpdateContext child_context =
        !layout_object || fragment.IsAnonymousBox()
            ? UpdateContext{context.parent_box,
                            context.offset_to_parent_box + fragment.Offset()}
            : UpdateContext{layout_object};
    for (auto& child : children_) {
      child->UpdateVisualRectFromLayoutObject(child_context);
    }
  }
}

void NGPaintFragment::AddOutlineRects(
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset) const {
  DCHECK(outline_rects);

  // TODO(layout-dev): This isn't correct but is close enough until we have
  // the right set of rects for outlines.
  for (const auto& child : children_) {
    LayoutRect outline_rect = child->VisualRect();
    outline_rect.MoveBy(additional_offset);
    outline_rects->push_back(outline_rect);
  }
}

void NGPaintFragment::PaintInlineBoxForDescendants(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutInline* layout_object,
    NGPhysicalOffset offset) const {
  for (const auto& child : Children()) {
    if (child->GetLayoutObject() == layout_object) {
      NGBoxFragmentPainter(*child).PaintInlineBox(
          paint_info, paint_offset + offset.ToLayoutPoint(), paint_offset);
      continue;
    }

    child->PaintInlineBoxForDescendants(paint_info, paint_offset, layout_object,
                                        offset + child->Offset());
  }
}

}  // namespace blink

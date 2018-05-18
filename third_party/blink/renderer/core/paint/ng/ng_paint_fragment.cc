// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"

namespace blink {

namespace {

NGPhysicalOffsetRect ExpandedSelectionRectForLineBreakIfNeeded(
    const NGPhysicalOffsetRect& rect,
    const NGPaintFragment& paint_fragment,
    const LayoutSelectionStatus& selection_status) {
  // Expand paint rect if selection covers multiple lines and
  // this fragment is at the end of line.
  if (selection_status.line_break == SelectLineBreak::kNotSelected)
    return rect;
  if (paint_fragment.GetLayoutObject()
          ->EnclosingNGBlockFlow()
          ->ShouldTruncateOverflowingText())
    return rect;
  // Copy from InlineTextBoxPainter.
  const NGPaintFragment* container_line_box = paint_fragment.ContainerLineBox();
  DCHECK(container_line_box);
  const NGPhysicalLineBoxFragment& physical_line_box =
      ToNGPhysicalLineBoxFragment(container_line_box->PhysicalFragment());
  const LayoutUnit space_width(paint_fragment.Style().GetFont().SpaceWidth());
  const NGPhysicalSize expanded_size(rect.size.width + space_width,
                                     rect.size.height);
  // TODO(yoichio): Support vertical writing mode.
  // Consider sharing physical directional algorithm with ng_caret_rect.cc.
  if (IsLtr(physical_line_box.BaseDirection()))
    return NGPhysicalOffsetRect(rect.offset, expanded_size);
  return NGPhysicalOffsetRect(
      NGPhysicalOffset(rect.offset.left - space_width, rect.offset.top),
      expanded_size);
}

}  // namespace

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

NGPaintFragment* NGPaintFragment::GetForInlineContainer(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline());
  // Search from its parent because |EnclosingNGBlockFlow| returns itself when
  // the LayoutObject is a box (i.e., atomic inline, including inline block and
  // replaced elements.)
  if (LayoutObject* parent = layout_object->Parent()) {
    if (LayoutBlockFlow* block_flow = parent->EnclosingNGBlockFlow())
      return block_flow->PaintFragment();
  }
  return nullptr;
}

NGPaintFragment::FragmentRange NGPaintFragment::InlineFragmentsFor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline());
  if (const NGPaintFragment* root = GetForInlineContainer(layout_object)) {
    auto it = root->first_fragment_map_.find(layout_object);
    if (it != root->first_fragment_map_.end())
      return FragmentRange(it->value);

    // Reaching here means that there are no fragments for the LayoutObject.
    // Culled inline box is one, but can be space-only LayoutText that were
    // collapsed out.
    return FragmentRange(nullptr);
  }
  return FragmentRange(nullptr, false);
}

bool NGPaintFragment::FlippedLocalVisualRectFor(
    const LayoutObject* layout_object,
    LayoutRect* visual_rect) {
  auto fragments = InlineFragmentsFor(layout_object);
  if (!fragments.IsInLayoutNGInlineFormattingContext())
    return false;

  for (NGPaintFragment* fragment : fragments) {
    NGPhysicalOffsetRect child_visual_rect =
        fragment->PhysicalFragment().SelfVisualRect();
    child_visual_rect.offset += fragment->InlineOffsetToContainerBox();
    visual_rect->Unite(child_visual_rect.ToLayoutRect());
  }
  if (!layout_object->HasFlippedBlocksWritingMode())
    return true;

  NGPaintFragment* container = GetForInlineContainer(layout_object);
  DCHECK(container);
  ToLayoutBox(container->GetLayoutObject())->FlipForWritingMode(*visual_rect);
  return true;
}

void NGPaintFragment::UpdateVisualRectForNonLayoutObjectChildren() {
  // Scan direct children only beause line boxes are always direct children of
  // the inline formatting context.
  for (auto& child : Children()) {
    if (!child->PhysicalFragment().IsLineBox())
      continue;
    LayoutRect union_of_children;
    for (const auto& descendant : child->Children())
      union_of_children.Unite(descendant->VisualRect());
    child->SetVisualRect(union_of_children);
  }
}

void NGPaintFragment::AddSelfOutlineRect(
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset) const {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (fragment.IsBox()) {
    ToNGPhysicalBoxFragment(fragment).AddSelfOutlineRects(outline_rects,
                                                          additional_offset);
  }
}

void NGPaintFragment::PaintInlineBoxForDescendants(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutInline* layout_object,
    NGPhysicalOffset offset) const {
  for (const auto& child : Children()) {
    if (child->GetLayoutObject() == layout_object) {
      NGInlineBoxFragmentPainter(*child).Paint(
          paint_info, paint_offset + offset.ToLayoutPoint() /*, paint_offset*/);
      continue;
    }

    child->PaintInlineBoxForDescendants(paint_info, paint_offset, layout_object,
                                        offset + child->Offset());
  }
}

LayoutRect NGPaintFragment::PartialInvalidationRect() const {
  // TODO(yochio): On SlimmingPaintV175, this function is used to invalidate old selected rect in
  // this fragment by PaintController::GenerateRasterInvalidation.
  // So far we just return enclosing block flow's visual rect to pass layout test
  // but this makes performance worse. We should return LayoutRect on that
  // ng_text_fragment_painter::PaintSelection paints selection.
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());
  const NGPaintFragment* block_fragment =
      GetLayoutObject()->EnclosingNGBlockFlow()->PaintFragment();
  return block_fragment->VisualRect();
}

const NGPaintFragment* NGPaintFragment::ContainerLineBox() const {
  DCHECK(PhysicalFragment().IsInline());
  for (const NGPaintFragment* runner = this; runner;
       runner = runner->Parent()) {
    if (runner->PhysicalFragment().IsLineBox())
      return runner;
  }
  NOTREACHED();
  return nullptr;
}

NGPaintFragment* NGPaintFragment::FirstLineBox() const {
  for (auto& child : children_) {
    if (child->PhysicalFragment().IsLineBox())
      return child.get();
  }
  return nullptr;
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationRecursively() {
  if (LayoutObject* layout_object = GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();

  for (auto& child : children_)
    child->SetShouldDoFullPaintInvalidationRecursively();
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationForFirstLine() {
  DCHECK(PhysicalFragment().IsBox() && GetLayoutObject() &&
         GetLayoutObject()->IsLayoutBlockFlow());

  if (NGPaintFragment* line_box = FirstLineBox())
    line_box->SetShouldDoFullPaintInvalidationRecursively();
}

NGPhysicalOffsetRect NGPaintFragment::ComputeLocalSelectionRect(
    const LayoutSelectionStatus& selection_status) const {
  const NGPhysicalOffsetRect& selection_rect =
      ToNGPhysicalTextFragmentOrDie(PhysicalFragment())
          .LocalRect(selection_status.start, selection_status.end);
  const NGPhysicalOffsetRect line_break_extended_rect =
      ExpandedSelectionRectForLineBreakIfNeeded(selection_rect, *this,
                                                selection_status);
  return line_break_extended_rect;
}

}  // namespace blink

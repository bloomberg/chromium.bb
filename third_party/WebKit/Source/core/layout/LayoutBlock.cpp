/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "core/layout/LayoutBlock.h"

#include <memory>
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/StyleEngine.h"
#include "core/editing/DragCaret.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMarqueeElement.h"
#include "core/layout/HitTestLocation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/Page.h"
#include "core/paint/BlockPaintInvalidator.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

struct SameSizeAsLayoutBlock : public LayoutBox {
  LayoutObjectChildList children;
  uint32_t bitfields;
};

static_assert(sizeof(LayoutBlock) == sizeof(SameSizeAsLayoutBlock),
              "LayoutBlock should stay small");

// This map keeps track of the positioned objects associated with a containing
// block.
//
// This map is populated during layout. It is kept across layouts to handle
// that we skip unchanged sub-trees during layout, in such a way that we are
// able to lay out deeply nested out-of-flow descendants if their containing
// block got laid out. The map could be invalidated during style change but
// keeping track of containing blocks at that time is complicated (we are in
// the middle of recomputing the style so we can't rely on any of its
// information), which is why it's easier to just update it for every layout.
static TrackedDescendantsMap* g_positioned_descendants_map = nullptr;
static TrackedContainerMap* g_positioned_container_map = nullptr;

// This map keeps track of the descendants whose 'height' is percentage
// associated with a containing block. Like |gPositionedDescendantsMap|, it is
// also recomputed for every layout (see the comment above about why).
static TrackedDescendantsMap* g_percent_height_descendants_map = nullptr;

LayoutBlock::LayoutBlock(ContainerNode* node)
    : LayoutBox(node),
      has_margin_before_quirk_(false),
      has_margin_after_quirk_(false),
      being_destroyed_(false),
      has_markup_truncation_(false),
      width_available_to_children_changed_(false),
      height_available_to_children_changed_(false),
      is_self_collapsing_(false),
      descendants_with_floats_marked_for_layout_(false),
      has_positioned_objects_(false),
      has_percent_height_descendants_(false),
      pagination_state_changed_(false) {
  // LayoutBlockFlow calls setChildrenInline(true).
  // By default, subclasses do not have inline children.
}

void LayoutBlock::RemoveFromGlobalMaps() {
  if (HasPositionedObjects()) {
    std::unique_ptr<TrackedLayoutBoxListHashSet> descendants =
        g_positioned_descendants_map->Take(this);
    DCHECK(!descendants->IsEmpty());
    for (LayoutBox* descendant : *descendants) {
      DCHECK_EQ(g_positioned_container_map->at(descendant), this);
      g_positioned_container_map->erase(descendant);
    }
  }
  if (HasPercentHeightDescendants()) {
    std::unique_ptr<TrackedLayoutBoxListHashSet> descendants =
        g_percent_height_descendants_map->Take(this);
    DCHECK(!descendants->IsEmpty());
    for (LayoutBox* descendant : *descendants) {
      DCHECK_EQ(descendant->PercentHeightContainer(), this);
      descendant->SetPercentHeightContainer(nullptr);
    }
  }
}

LayoutBlock::~LayoutBlock() {
  RemoveFromGlobalMaps();
}

void LayoutBlock::WillBeDestroyed() {
  if (!DocumentBeingDestroyed() && Parent())
    Parent()->DirtyLinesFromChangedChild(this);

  if (LocalFrame* frame = this->GetFrame()) {
    frame->Selection().LayoutBlockWillBeDestroyed(*this);
    frame->GetPage()->GetDragCaret().LayoutBlockWillBeDestroyed(*this);
  }

  if (TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer())
    text_autosizer->Destroy(this);

  LayoutBox::WillBeDestroyed();
}

void LayoutBlock::StyleWillChange(StyleDifference diff,
                                  const ComputedStyle& new_style) {
  const ComputedStyle* old_style = Style();

  SetIsAtomicInlineLevel(new_style.IsDisplayInlineType());

  if (old_style && Parent()) {
    bool old_style_contains_fixed_position =
        old_style->CanContainFixedPositionObjects();
    bool old_style_contains_absolute_position =
        old_style_contains_fixed_position ||
        old_style->CanContainAbsolutePositionObjects();
    bool new_style_contains_fixed_position =
        new_style.CanContainFixedPositionObjects();
    bool new_style_contains_absolute_position =
        new_style_contains_fixed_position ||
        new_style.CanContainAbsolutePositionObjects();

    if ((old_style_contains_fixed_position &&
         !new_style_contains_fixed_position) ||
        (old_style_contains_absolute_position &&
         !new_style_contains_absolute_position)) {
      // Clear our positioned objects list. Our absolute and fixed positioned
      // descendants will be inserted into our containing block's positioned
      // objects list during layout.
      RemovePositionedObjects(nullptr, kNewContainingBlock);
    }
    if (!old_style_contains_absolute_position &&
        new_style_contains_absolute_position) {
      // Remove our absolutely positioned descendants from their current
      // containing block.
      // They will be inserted into our positioned objects list during layout.
      if (LayoutBlock* cb = ContainingBlockForAbsolutePosition())
        cb->RemovePositionedObjects(this, kNewContainingBlock);
    }
    if (!old_style_contains_fixed_position &&
        new_style_contains_fixed_position) {
      // Remove our fixed positioned descendants from their current containing
      // block.
      // They will be inserted into our positioned objects list during layout.
      if (LayoutBlock* cb = ContainerForFixedPosition())
        cb->RemovePositionedObjects(this, kNewContainingBlock);
    }
  }

  LayoutBox::StyleWillChange(diff, new_style);
}

enum LogicalExtent { kLogicalWidth, kLogicalHeight };
static bool BorderOrPaddingLogicalDimensionChanged(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style,
    LogicalExtent logical_extent) {
  if (new_style.IsHorizontalWritingMode() ==
      (logical_extent == kLogicalWidth)) {
    return old_style.BorderLeftWidth() != new_style.BorderLeftWidth() ||
           old_style.BorderRightWidth() != new_style.BorderRightWidth() ||
           old_style.PaddingLeft() != new_style.PaddingLeft() ||
           old_style.PaddingRight() != new_style.PaddingRight();
  }

  return old_style.BorderTopWidth() != new_style.BorderTopWidth() ||
         old_style.BorderBottomWidth() != new_style.BorderBottomWidth() ||
         old_style.PaddingTop() != new_style.PaddingTop() ||
         old_style.PaddingBottom() != new_style.PaddingBottom();
}

void LayoutBlock::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  LayoutBox::StyleDidChange(diff, old_style);

  const ComputedStyle& new_style = StyleRef();

  if (old_style && Parent()) {
    if (old_style->GetPosition() != new_style.GetPosition() &&
        new_style.GetPosition() != EPosition::kStatic) {
      // In LayoutObject::styleWillChange() we already removed ourself from our
      // old containing block's positioned descendant list, and we will be
      // inserted to the new containing block's list during layout. However the
      // positioned descendant layout logic assumes layout objects to obey
      // parent-child order in the list. Remove our descendants here so they
      // will be re-inserted after us.
      if (LayoutBlock* cb = ContainingBlock()) {
        cb->RemovePositionedObjects(this, kNewContainingBlock);
        if (IsOutOfFlowPositioned()) {
          // Insert this object into containing block's positioned descendants
          // list in case the parent won't layout. This is needed especially
          // there are descendants scheduled for overflow recalc.
          cb->InsertPositionedObject(this);
        }
      }
    }
  }

  if (TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer())
    text_autosizer->Record(this);

  PropagateStyleToAnonymousChildren();

  // The LayoutView is always a container of fixed positioned descendants. In
  // addition, SVG foreignObjects become such containers, so that descendants
  // of a foreignObject cannot escape it. Similarly, text controls let authors
  // select elements inside that are created by user agent shadow DOM, and we
  // have (C++) code that assumes that the elements are indeed contained by the
  // text control. So just make sure this is the case. Finally, computed style
  // may turn us into a container of all things, e.g. if the element is
  // transformed, or contain:paint is specified.
  SetCanContainFixedPositionObjects(IsLayoutView() || IsSVGForeignObject() ||
                                    IsTextControl() ||
                                    new_style.CanContainFixedPositionObjects());

  // It's possible for our border/padding to change, but for the overall logical
  // width or height of the block to end up being the same. We keep track of
  // this change so in layoutBlock, we can know to set relayoutChildren=true.
  width_available_to_children_changed_ |=
      old_style && NeedsLayout() &&
      (diff.NeedsFullLayout() || BorderOrPaddingLogicalDimensionChanged(
                                     *old_style, new_style, kLogicalWidth));
  height_available_to_children_changed_ |=
      old_style && diff.NeedsFullLayout() && NeedsLayout() &&
      BorderOrPaddingLogicalDimensionChanged(*old_style, new_style,
                                             kLogicalHeight);
}

void LayoutBlock::UpdateFromStyle() {
  LayoutBox::UpdateFromStyle();

  bool should_clip_overflow =
      !StyleRef().IsOverflowVisible() && AllowsOverflowClip();
  if (should_clip_overflow != HasOverflowClip()) {
    if (!should_clip_overflow)
      GetScrollableArea()->InvalidateAllStickyConstraints();
    SetMayNeedPaintInvalidationSubtree();
    // The overflow clip paint property depends on whether overflow clip is
    // present so we need to update paint properties if this changes.
    SetNeedsPaintPropertyUpdate();
  }
  SetHasOverflowClip(should_clip_overflow);
}

bool LayoutBlock::AllowsOverflowClip() const {
  // If overflow has been propagated to the viewport, it has no effect here.
  return GetNode() != GetDocument().ViewportDefiningElement();
}

void LayoutBlock::AddChildBeforeDescendant(LayoutObject* new_child,
                                           LayoutObject* before_descendant) {
  DCHECK_NE(before_descendant->Parent(), this);
  LayoutObject* before_descendant_container = before_descendant->Parent();
  while (before_descendant_container->Parent() != this)
    before_descendant_container = before_descendant_container->Parent();
  DCHECK(before_descendant_container);

  // We really can't go on if what we have found isn't anonymous. We're not
  // supposed to use some random non-anonymous object and put the child there.
  // That's a recipe for security issues.
  CHECK(before_descendant_container->IsAnonymous());

  // If the requested insertion point is not one of our children, then this is
  // because there is an anonymous container within this object that contains
  // the beforeDescendant.
  if (before_descendant_container->IsAnonymousBlock()
      // Full screen layoutObjects and full screen placeholders act as anonymous
      // blocks, not tables:
      || before_descendant_container->IsLayoutFullScreen() ||
      before_descendant_container->IsLayoutFullScreenPlaceholder()) {
    // Insert the child into the anonymous block box instead of here.
    if (new_child->IsInline() || new_child->IsFloatingOrOutOfFlowPositioned() ||
        before_descendant->Parent()->SlowFirstChild() != before_descendant)
      before_descendant->Parent()->AddChild(new_child, before_descendant);
    else
      AddChild(new_child, before_descendant->Parent());
    return;
  }

  DCHECK(before_descendant_container->IsTable());
  if (new_child->IsTablePart()) {
    // Insert into the anonymous table.
    before_descendant_container->AddChild(new_child, before_descendant);
    return;
  }

  LayoutObject* before_child =
      SplitAnonymousBoxesAroundChild(before_descendant);

  DCHECK_EQ(before_child->Parent(), this);
  if (before_child->Parent() != this) {
    // We should never reach here. If we do, we need to use the
    // safe fallback to use the topmost beforeChild container.
    before_child = before_descendant_container;
  }

  AddChild(new_child, before_child);
}

void LayoutBlock::AddChild(LayoutObject* new_child,
                           LayoutObject* before_child) {
  if (before_child && before_child->Parent() != this) {
    AddChildBeforeDescendant(new_child, before_child);
    return;
  }

  // Only LayoutBlockFlow should have inline children, and then we shouldn't be
  // here.
  DCHECK(!ChildrenInline());

  if (new_child->IsInline() || new_child->IsFloatingOrOutOfFlowPositioned()) {
    // If we're inserting an inline child but all of our children are blocks,
    // then we have to make sure it is put into an anomyous block box. We try to
    // use an existing anonymous box if possible, otherwise a new one is created
    // and inserted into our list of children in the appropriate position.
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : LastChild();

    if (after_child && after_child->IsAnonymousBlock()) {
      after_child->AddChild(new_child);
      return;
    }

    if (new_child->IsInline()) {
      // No suitable existing anonymous box - create a new one.
      LayoutBlock* new_box = CreateAnonymousBlock();
      LayoutBox::AddChild(new_box, before_child);
      new_box->AddChild(new_child);
      return;
    }
  }

  LayoutBox::AddChild(new_child, before_child);
}

void LayoutBlock::RemoveLeftoverAnonymousBlock(LayoutBlock* child) {
  DCHECK(child->IsAnonymousBlock());
  DCHECK(!child->ChildrenInline());
  DCHECK_EQ(child->Parent(), this);

  if (child->Continuation())
    return;

  // Promote all the leftover anonymous block's children (to become children of
  // this block instead). We still want to keep the leftover block in the tree
  // for a moment, for notification purposes done further below (flow threads
  // and grids).
  child->MoveAllChildrenTo(this, child->NextSibling());

  // Remove all the information in the flow thread associated with the leftover
  // anonymous block.
  child->RemoveFromLayoutFlowThread();

  // LayoutGrid keeps track of its children, we must notify it about changes in
  // the tree.
  if (child->Parent()->IsLayoutGrid())
    ToLayoutGrid(child->Parent())->DirtyGrid();

  // Now remove the leftover anonymous block from the tree, and destroy it.
  // We'll rip it out manually from the tree before destroying it, because we
  // don't want to trigger any tree adjustments with regards to anonymous blocks
  // (or any other kind of undesired chain-reaction).
  Children()->RemoveChildNode(this, child, false);
  child->Destroy();
}

void LayoutBlock::UpdateAfterLayout() {
  InvalidateStickyConstraints();
  LayoutBox::UpdateAfterLayout();
}

void LayoutBlock::UpdateLayout() {
  DCHECK(!GetScrollableArea() || GetScrollableArea()->GetScrollAnchor());

  LayoutAnalyzer::Scope analyzer(*this);

  bool needs_scroll_anchoring =
      HasOverflowClip() && GetScrollableArea()->ShouldPerformScrollAnchoring();
  if (needs_scroll_anchoring)
    GetScrollableArea()->GetScrollAnchor()->NotifyBeforeLayout();

  // Table cells call layoutBlock directly, so don't add any logic here.  Put
  // code into layoutBlock().
  UpdateBlockLayout(false);

  // It's safe to check for control clip here, since controls can never be table
  // cells. If we have a lightweight clip, there can never be any overflow from
  // children.
  if (HasControlClip() && overflow_)
    ClearLayoutOverflow();

  InvalidateBackgroundObscurationStatus();
  height_available_to_children_changed_ = false;
}

bool LayoutBlock::WidthAvailableToChildrenHasChanged() {
  // TODO(robhogan): Does m_widthAvailableToChildrenChanged always get reset
  // when it needs to?
  bool width_available_to_children_has_changed =
      width_available_to_children_changed_;
  width_available_to_children_changed_ = false;

  // If we use border-box sizing, have percentage padding, and our parent has
  // changed width then the width available to our children has changed even
  // though our own width has remained the same.
  // TODO(mstensho): NeedsPreferredWidthsRecalculation() is used here to check
  // if we have percentage padding, which is rather non-obvious. That method
  // returns true in other cases as well.
  width_available_to_children_has_changed |=
      Style()->BoxSizing() == EBoxSizing::kBorderBox &&
      NeedsPreferredWidthsRecalculation() &&
      View()->GetLayoutState()->ContainingBlockLogicalWidthChanged();

  return width_available_to_children_has_changed;
}

DISABLE_CFI_PERF
bool LayoutBlock::UpdateLogicalWidthAndColumnWidth() {
  LayoutUnit old_width = LogicalWidth();
  UpdateLogicalWidth();
  return old_width != LogicalWidth() || WidthAvailableToChildrenHasChanged();
}

void LayoutBlock::UpdateBlockLayout(bool) {
  NOTREACHED();
  ClearNeedsLayout();
}

void LayoutBlock::AddOverflowFromChildren() {
  if (ChildrenInline())
    ToLayoutBlockFlow(this)->AddOverflowFromInlineChildren();
  else
    AddOverflowFromBlockChildren();
}

DISABLE_CFI_PERF
void LayoutBlock::ComputeOverflow(LayoutUnit old_client_after_edge, bool) {
  overflow_.reset();

  AddOverflowFromChildren();
  AddOverflowFromPositionedObjects();

  if (HasOverflowClip()) {
    // When we have overflow clip, propagate the original spillout since it will
    // include collapsed bottom margins and bottom padding. Set the axis we
    // don't care about to be 1, since we want this overflow to always be
    // considered reachable.
    LayoutRect client_rect(NoOverflowRect());
    LayoutRect rect_to_apply;
    if (IsHorizontalWritingMode())
      rect_to_apply = LayoutRect(
          client_rect.X(), client_rect.Y(), LayoutUnit(1),
          (old_client_after_edge - client_rect.Y()).ClampNegativeToZero());
    else
      rect_to_apply = LayoutRect(
          client_rect.X(), client_rect.Y(),
          (old_client_after_edge - client_rect.X()).ClampNegativeToZero(),
          LayoutUnit(1));
    AddLayoutOverflow(rect_to_apply);
    if (HasOverflowModel())
      overflow_->SetLayoutClientAfterEdge(old_client_after_edge);
  }

  AddVisualEffectOverflow();
  AddVisualOverflowFromTheme();

  // An enclosing composited layer will need to update its bounds if we now
  // overflow it.
  PaintLayer* layer = EnclosingLayer();
  if (!NeedsLayout() && layer->HasCompositedLayerMapping() &&
      !layer->VisualRect().Contains(VisualOverflowRect()))
    layer->SetNeedsCompositingInputsUpdate();
}

void LayoutBlock::AddOverflowFromBlockChildren() {
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsFloatingOrOutOfFlowPositioned() || child->IsColumnSpanAll())
      continue;

    // If the child contains inline with outline and continuation, its
    // visual overflow computed during its layout might be inaccurate because
    // the layout of continuations might not be up-to-date at that time.
    // Re-add overflow from inline children to ensure its overflow covers
    // the outline which may enclose continuations.
    if (child->IsLayoutBlockFlow() &&
        ToLayoutBlockFlow(child)->ContainsInlineWithOutlineAndContinuation())
      ToLayoutBlockFlow(child)->AddOverflowFromInlineChildren();

    AddOverflowFromChild(*child);
  }
}

void LayoutBlock::AddOverflowFromPositionedObjects() {
  TrackedLayoutBoxListHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  for (auto* positioned_object : *positioned_descendants) {
    // Fixed positioned elements don't contribute to layout overflow, since they
    // don't scroll with the content.
    if (positioned_object->Style()->GetPosition() != EPosition::kFixed)
      AddOverflowFromChild(*positioned_object,
                           ToLayoutSize(positioned_object->Location()));
  }
}

void LayoutBlock::AddVisualOverflowFromTheme() {
  if (!Style()->HasAppearance())
    return;

  IntRect inflated_rect = PixelSnappedBorderBoxRect();
  LayoutTheme::GetTheme().AddVisualOverflow(*this, inflated_rect);
  AddSelfVisualOverflow(LayoutRect(inflated_rect));
}

static inline bool ChangeInAvailableLogicalHeightAffectsChild(
    LayoutBlock* parent,
    LayoutBox& child) {
  if (parent->Style()->BoxSizing() != EBoxSizing::kBorderBox)
    return false;
  return parent->Style()->IsHorizontalWritingMode() &&
         !child.Style()->IsHorizontalWritingMode();
}

void LayoutBlock::UpdateBlockChildDirtyBitsBeforeLayout(bool relayout_children,
                                                        LayoutBox& child) {
  if (child.IsOutOfFlowPositioned()) {
    // It's rather useless to mark out-of-flow children at this point. We may
    // not be their containing block (and if we are, it's just pure luck), so
    // this would be the wrong place for it. Furthermore, it would cause trouble
    // for out-of-flow descendants of column spanners, if the containing block
    // is outside the spanner but inside the multicol container.
    return;
  }
  // FIXME: Technically percentage height objects only need a relayout if their
  // percentage isn't going to be turned into an auto value. Add a method to
  // determine this, so that we can avoid the relayout.
  bool has_relative_logical_height =
      child.HasRelativeLogicalHeight() ||
      (child.IsAnonymous() && this->HasRelativeLogicalHeight()) ||
      child.StretchesToViewport();
  if (relayout_children || (has_relative_logical_height && !IsLayoutView()) ||
      (height_available_to_children_changed_ &&
       ChangeInAvailableLogicalHeightAffectsChild(this, child))) {
    child.SetChildNeedsLayout(kMarkOnlyThis);
  }
}

void LayoutBlock::SimplifiedNormalFlowLayout() {
  if (ChildrenInline()) {
    SECURITY_DCHECK(IsLayoutBlockFlow());
    LayoutBlockFlow* block_flow = ToLayoutBlockFlow(this);
    block_flow->SimplifiedNormalFlowInlineLayout();
  } else {
    for (LayoutBox* box = FirstChildBox(); box; box = box->NextSiblingBox()) {
      if (!box->IsOutOfFlowPositioned()) {
        if (box->IsLayoutMultiColumnSpannerPlaceholder())
          ToLayoutMultiColumnSpannerPlaceholder(box)
              ->MarkForLayoutIfObjectInFlowThreadNeedsLayout();
        box->LayoutIfNeeded();
      }
    }
  }
}

bool LayoutBlock::SimplifiedLayout() {
  // Check if we need to do a full layout.
  if (NormalChildNeedsLayout() || SelfNeedsLayout())
    return false;

  // Check that we actually need to do a simplified layout.
  if (!PosChildNeedsLayout() &&
      !(NeedsSimplifiedNormalFlowLayout() || NeedsPositionedMovementLayout()))
    return false;

  {
    // LayoutState needs this deliberate scope to pop before paint invalidation.
    LayoutState state(*this);

    if (NeedsPositionedMovementLayout() &&
        !TryLayoutDoingPositionedMovementOnly())
      return false;

    if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock()) {
      if (!flow_thread->CanSkipLayout(*this))
        return false;
    }

    TextAutosizer::LayoutScope text_autosizer_layout_scope(this);

    // Lay out positioned descendants or objects that just need to recompute
    // overflow.
    if (NeedsSimplifiedNormalFlowLayout())
      SimplifiedNormalFlowLayout();

    // Lay out our positioned objects if our positioned child bit is set.
    // Also, if an absolute position element inside a relative positioned
    // container moves, and the absolute element has a fixed position child
    // neither the fixed element nor its container learn of the movement since
    // posChildNeedsLayout() is only marked as far as the relative positioned
    // container. So if we can have fixed pos objects in our positioned objects
    // list check if any of them are statically positioned and thus need to move
    // with their absolute ancestors.
    bool can_contain_fixed_pos_objects = CanContainFixedPositionObjects();
    if (PosChildNeedsLayout() || NeedsPositionedMovementLayout() ||
        can_contain_fixed_pos_objects)
      LayoutPositionedObjects(
          false, NeedsPositionedMovementLayout()
                     ? kForcedLayoutAfterContainingBlockMoved
                     : (!PosChildNeedsLayout() && can_contain_fixed_pos_objects
                            ? kLayoutOnlyFixedPositionedObjects
                            : kDefaultLayout));

    // Recompute our overflow information.
    // FIXME: We could do better here by computing a temporary overflow object
    // from layoutPositionedObjects and only updating our overflow if we either
    // used to have overflow or if the new temporary object has overflow.
    // For now just always recompute overflow. This is no worse performance-wise
    // than the old code that called rightmostPosition and lowestPosition on
    // every relayout so it's not a regression. computeOverflow expects the
    // bottom edge before we clamp our height. Since this information isn't
    // available during simplifiedLayout, we cache the value in m_overflow.
    LayoutUnit old_client_after_edge = HasOverflowModel()
                                           ? overflow_->LayoutClientAfterEdge()
                                           : ClientLogicalBottom();
    ComputeOverflow(old_client_after_edge, true);
  }

  UpdateAfterLayout();

  ClearNeedsLayout();

  if (LayoutAnalyzer* analyzer = GetFrameView()->GetLayoutAnalyzer())
    analyzer->Increment(LayoutAnalyzer::kLayoutObjectsThatNeedSimplifiedLayout);

  return true;
}

void LayoutBlock::MarkFixedPositionObjectForLayoutIfNeeded(
    LayoutObject* child,
    SubtreeLayoutScope& layout_scope) {
  if (child->Style()->GetPosition() != EPosition::kFixed)
    return;

  bool has_static_block_position =
      child->Style()->HasStaticBlockPosition(IsHorizontalWritingMode());
  bool has_static_inline_position =
      child->Style()->HasStaticInlinePosition(IsHorizontalWritingMode());
  if (!has_static_block_position && !has_static_inline_position)
    return;

  LayoutObject* o = child->Parent();
  while (o && !o->IsLayoutView() &&
         o->Style()->GetPosition() != EPosition::kAbsolute)
    o = o->Parent();
  // The LayoutView is absolute-positioned, but does not move.
  if (o->IsLayoutView())
    return;

  // We must compute child's width and height, but not update them now.
  // The child will update its width and height when it gets laid out, and needs
  // to see them change there.
  LayoutBox* box = ToLayoutBox(child);
  if (has_static_inline_position) {
    LogicalExtentComputedValues computed_values;
    box->ComputeLogicalWidth(computed_values);
    LayoutUnit new_left = computed_values.position_;
    if (new_left != box->LogicalLeft())
      layout_scope.SetChildNeedsLayout(child);
  }
  if (has_static_block_position) {
    LogicalExtentComputedValues computed_values;
    box->ComputeLogicalHeight(computed_values);
    LayoutUnit new_top = computed_values.position_;
    if (new_top != box->LogicalTop())
      layout_scope.SetChildNeedsLayout(child);
  }
}

LayoutUnit LayoutBlock::MarginIntrinsicLogicalWidthForChild(
    const LayoutBox& child) const {
  // A margin has three types: fixed, percentage, and auto (variable).
  // Auto and percentage margins become 0 when computing min/max width.
  // Fixed margins can be added in as is.
  Length margin_left = child.StyleRef().MarginStartUsing(StyleRef());
  Length margin_right = child.StyleRef().MarginEndUsing(StyleRef());
  LayoutUnit margin;
  if (margin_left.IsFixed())
    margin += margin_left.Value();
  if (margin_right.IsFixed())
    margin += margin_right.Value();
  return margin;
}

static bool NeedsLayoutDueToStaticPosition(LayoutBox* child) {
  // When a non-positioned block element moves, it may have positioned children
  // that are implicitly positioned relative to the non-positioned block.
  const ComputedStyle* style = child->Style();
  bool is_horizontal = style->IsHorizontalWritingMode();
  if (style->HasStaticBlockPosition(is_horizontal)) {
    LayoutBox::LogicalExtentComputedValues computed_values;
    LayoutUnit current_logical_top = child->LogicalTop();
    LayoutUnit current_logical_height = child->LogicalHeight();
    child->ComputeLogicalHeight(current_logical_height, current_logical_top,
                                computed_values);
    if (computed_values.position_ != current_logical_top ||
        computed_values.extent_ != current_logical_height)
      return true;
  }
  if (style->HasStaticInlinePosition(is_horizontal)) {
    LayoutBox::LogicalExtentComputedValues computed_values;
    LayoutUnit current_logical_left = child->LogicalLeft();
    LayoutUnit current_logical_width = child->LogicalWidth();
    child->ComputeLogicalWidth(computed_values);
    if (computed_values.position_ != current_logical_left ||
        computed_values.extent_ != current_logical_width)
      return true;
  }
  return false;
}

void LayoutBlock::LayoutPositionedObjects(bool relayout_children,
                                          PositionedLayoutBehavior info) {
  TrackedLayoutBoxListHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  for (auto* positioned_object : *positioned_descendants) {
    LayoutPositionedObject(positioned_object, relayout_children, info);
  }
}

void LayoutBlock::LayoutPositionedObject(LayoutBox* positioned_object,
                                         bool relayout_children,
                                         PositionedLayoutBehavior info) {
  positioned_object->SetMayNeedPaintInvalidation();

  SubtreeLayoutScope layout_scope(*positioned_object);
  // If positionedObject is fixed-positioned and moves with an absolute-
  // positioned ancestor (other than the LayoutView, which cannot move),
  // mark it for layout now.
  MarkFixedPositionObjectForLayoutIfNeeded(positioned_object, layout_scope);
  if (info == kLayoutOnlyFixedPositionedObjects) {
    positioned_object->LayoutIfNeeded();
    return;
  }

  if (!positioned_object->NormalChildNeedsLayout() &&
      (relayout_children || height_available_to_children_changed_ ||
       NeedsLayoutDueToStaticPosition(positioned_object)))
    layout_scope.SetChildNeedsLayout(positioned_object);

  LayoutUnit logical_top_estimate;
  bool is_paginated = View()->GetLayoutState()->IsPaginated();
  bool needs_block_direction_location_set_before_layout =
      is_paginated &&
      positioned_object->GetPaginationBreakability() != kForbidBreaks;
  if (needs_block_direction_location_set_before_layout) {
    // Out-of-flow objects are normally positioned after layout (while in-flow
    // objects are positioned before layout). If the child object is paginated
    // in the same context as we are, estimate its logical top now. We need to
    // know this up-front, to correctly evaluate if we need to mark for
    // relayout, and, if our estimate is correct, we'll even be able to insert
    // correct pagination struts on the first attempt.
    LogicalExtentComputedValues computed_values;
    positioned_object->ComputeLogicalHeight(positioned_object->LogicalHeight(),
                                            positioned_object->LogicalTop(),
                                            computed_values);
    logical_top_estimate = computed_values.position_;
    positioned_object->SetLogicalTop(logical_top_estimate);
  }

  if (!positioned_object->NeedsLayout())
    MarkChildForPaginationRelayoutIfNeeded(*positioned_object, layout_scope);

  // FIXME: We should be able to do a r->setNeedsPositionedMovementLayout()
  // here instead of a full layout. Need to investigate why it does not
  // trigger the correct invalidations in that case. crbug.com/350756
  if (info == kForcedLayoutAfterContainingBlockMoved) {
    positioned_object->SetNeedsLayout(LayoutInvalidationReason::kAncestorMoved,
                                      kMarkOnlyThis);
  }

  positioned_object->LayoutIfNeeded();

  LayoutObject* parent = positioned_object->Parent();
  bool layout_changed = false;
  if (parent->IsFlexibleBox() &&
      ToLayoutFlexibleBox(parent)->SetStaticPositionForPositionedLayout(
          *positioned_object)) {
    // The static position of an abspos child of a flexbox depends on its size
    // (for example, they can be centered). So we may have to reposition the
    // item after layout.
    // TODO(cbiesinger): We could probably avoid a layout here and just
    // reposition?
    positioned_object->ForceChildLayout();
    layout_changed = true;
  }

  // Lay out again if our estimate was wrong.
  if (!layout_changed && needs_block_direction_location_set_before_layout &&
      logical_top_estimate != LogicalTopForChild(*positioned_object))
    positioned_object->ForceChildLayout();

  if (is_paginated)
    UpdateFragmentationInfoForChild(*positioned_object);
}

void LayoutBlock::MarkPositionedObjectsForLayout() {
  if (TrackedLayoutBoxListHashSet* positioned_descendants =
          PositionedObjects()) {
    for (auto* descendant : *positioned_descendants)
      descendant->SetChildNeedsLayout();
  }
}

void LayoutBlock::Paint(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset) const {
  BlockPainter(*this).Paint(paint_info, paint_offset);
}

void LayoutBlock::PaintChildren(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) const {
  BlockPainter(*this).PaintChildren(paint_info, paint_offset);
}

void LayoutBlock::PaintObject(const PaintInfo& paint_info,
                              const LayoutPoint& paint_offset) const {
  BlockPainter(*this).PaintObject(paint_info, paint_offset);
}

LayoutUnit LayoutBlock::BlockDirectionOffset(
    const LayoutSize& offset_from_block) const {
  return IsHorizontalWritingMode() ? offset_from_block.Height()
                                   : offset_from_block.Width();
}

LayoutUnit LayoutBlock::InlineDirectionOffset(
    const LayoutSize& offset_from_block) const {
  return IsHorizontalWritingMode() ? offset_from_block.Width()
                                   : offset_from_block.Height();
}

LayoutUnit LayoutBlock::LogicalLeftSelectionOffset(
    const LayoutBlock* root_block,
    LayoutUnit position) const {
  // The border can potentially be further extended by our containingBlock().
  if (root_block != this)
    return ContainingBlock()->LogicalLeftSelectionOffset(
        root_block, position + LogicalTop());
  return LogicalLeftOffsetForContent();
}

LayoutUnit LayoutBlock::LogicalRightSelectionOffset(
    const LayoutBlock* root_block,
    LayoutUnit position) const {
  // The border can potentially be further extended by our containingBlock().
  if (root_block != this)
    return ContainingBlock()->LogicalRightSelectionOffset(
        root_block, position + LogicalTop());
  return LogicalRightOffsetForContent();
}

void LayoutBlock::SetSelectionState(SelectionState state) {
  LayoutBox::SetSelectionState(state);

  if (InlineBoxWrapper() && CanUpdateSelectionOnRootLineBoxes()) {
    InlineBoxWrapper()->Root().SetHasSelectedChildren(state !=
                                                      SelectionState::kNone);
  }
}

TrackedLayoutBoxListHashSet* LayoutBlock::PositionedObjectsInternal() const {
  return g_positioned_descendants_map ? g_positioned_descendants_map->at(this)
                                      : nullptr;
}

void LayoutBlock::InsertPositionedObject(LayoutBox* o) {
  DCHECK(!IsAnonymousBlock());
  DCHECK_EQ(o->ContainingBlock(), this);

  o->ClearContainingBlockOverrideSize();

  if (g_positioned_container_map) {
    auto container_map_it = g_positioned_container_map->find(o);
    if (container_map_it != g_positioned_container_map->end()) {
      if (container_map_it->value == this) {
        DCHECK(HasPositionedObjects());
        DCHECK(PositionedObjects()->Contains(o));
        return;
      }
      RemovePositionedObject(o);
    }
  } else {
    g_positioned_container_map = new TrackedContainerMap;
  }
  g_positioned_container_map->Set(o, this);

  if (!g_positioned_descendants_map)
    g_positioned_descendants_map = new TrackedDescendantsMap;
  TrackedLayoutBoxListHashSet* descendant_set =
      g_positioned_descendants_map->at(this);
  if (!descendant_set) {
    descendant_set = new TrackedLayoutBoxListHashSet;
    g_positioned_descendants_map->Set(this, WTF::WrapUnique(descendant_set));
  }
  descendant_set->insert(o);

  has_positioned_objects_ = true;
}

void LayoutBlock::RemovePositionedObject(LayoutBox* o) {
  if (!g_positioned_container_map)
    return;

  LayoutBlock* container = g_positioned_container_map->Take(o);
  if (!container)
    return;

  TrackedLayoutBoxListHashSet* positioned_descendants =
      g_positioned_descendants_map->at(container);
  DCHECK(positioned_descendants);
  DCHECK(positioned_descendants->Contains(o));
  positioned_descendants->erase(o);
  if (positioned_descendants->IsEmpty()) {
    g_positioned_descendants_map->erase(container);
    container->has_positioned_objects_ = false;
  }
}

PaintInvalidationReason LayoutBlock::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  return BlockPaintInvalidator(*this).InvalidatePaint(context);
}

void LayoutBlock::ClearPreviousVisualRects() {
  LayoutBox::ClearPreviousVisualRects();
  BlockPaintInvalidator(*this).ClearPreviousVisualRects();
}

void LayoutBlock::RemovePositionedObjects(
    LayoutObject* o,
    ContainingBlockState containing_block_state) {
  TrackedLayoutBoxListHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  Vector<LayoutBox*, 16> dead_objects;
  for (auto* positioned_object : *positioned_descendants) {
    if (!o ||
        (positioned_object->IsDescendantOf(o) && o != positioned_object)) {
      if (containing_block_state == kNewContainingBlock) {
        positioned_object->SetChildNeedsLayout(kMarkOnlyThis);

        // The positioned object changing containing block may change paint
        // invalidation container.
        // Invalidate it (including non-compositing descendants) on its original
        // paint invalidation container.
        if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
          // This valid because we need to invalidate based on the current
          // status.
          DisableCompositingQueryAsserts compositing_disabler;
          if (!positioned_object->IsPaintInvalidationContainer())
            ObjectPaintInvalidator(*positioned_object)
                .InvalidatePaintIncludingNonCompositingDescendants();
        }
      }

      // It is parent blocks job to add positioned child to positioned objects
      // list of its containing block
      // Parent layout needs to be invalidated to ensure this happens.
      LayoutObject* p = positioned_object->Parent();
      while (p && !p->IsLayoutBlock())
        p = p->Parent();
      if (p)
        p->SetChildNeedsLayout();

      dead_objects.push_back(positioned_object);
    }
  }

  for (auto object : dead_objects) {
    DCHECK_EQ(g_positioned_container_map->at(object), this);
    positioned_descendants->erase(object);
    g_positioned_container_map->erase(object);
  }
  if (positioned_descendants->IsEmpty()) {
    g_positioned_descendants_map->erase(this);
    has_positioned_objects_ = false;
  }
}

void LayoutBlock::AddPercentHeightDescendant(LayoutBox* descendant) {
  if (descendant->PercentHeightContainer()) {
    if (descendant->PercentHeightContainer() == this) {
      DCHECK(HasPercentHeightDescendant(descendant));
      return;
    }
    descendant->RemoveFromPercentHeightContainer();
  }
  descendant->SetPercentHeightContainer(this);

  if (!g_percent_height_descendants_map)
    g_percent_height_descendants_map = new TrackedDescendantsMap;
  TrackedLayoutBoxListHashSet* descendant_set =
      g_percent_height_descendants_map->at(this);
  if (!descendant_set) {
    descendant_set = new TrackedLayoutBoxListHashSet;
    g_percent_height_descendants_map->Set(this,
                                          WTF::WrapUnique(descendant_set));
  }
  descendant_set->insert(descendant);

  has_percent_height_descendants_ = true;
}

void LayoutBlock::RemovePercentHeightDescendant(LayoutBox* descendant) {
  if (TrackedLayoutBoxListHashSet* descendants = PercentHeightDescendants()) {
    descendants->erase(descendant);
    descendant->SetPercentHeightContainer(nullptr);
    if (descendants->IsEmpty()) {
      g_percent_height_descendants_map->erase(this);
      has_percent_height_descendants_ = false;
    }
  }
}

TrackedLayoutBoxListHashSet* LayoutBlock::PercentHeightDescendantsInternal()
    const {
  return g_percent_height_descendants_map
             ? g_percent_height_descendants_map->at(this)
             : nullptr;
}

void LayoutBlock::DirtyForLayoutFromPercentageHeightDescendants(
    SubtreeLayoutScope& layout_scope) {
  TrackedLayoutBoxListHashSet* descendants = PercentHeightDescendants();
  if (!descendants)
    return;

  for (auto* box : *descendants) {
    DCHECK(box->IsDescendantOf(this));
    while (box != this) {
      if (box->NormalChildNeedsLayout())
        break;
      layout_scope.SetChildNeedsLayout(box);
      box = box->ContainingBlock();
      DCHECK(box);
      if (!box)
        break;
    }
  }
}

LayoutUnit LayoutBlock::TextIndentOffset() const {
  LayoutUnit cw;
  if (Style()->TextIndent().IsPercentOrCalc())
    cw = ContainingBlock()->AvailableLogicalWidth();
  return MinimumValueForLength(Style()->TextIndent(), cw);
}

bool LayoutBlock::IsPointInOverflowControl(
    HitTestResult& result,
    const LayoutPoint& location_in_container,
    const LayoutPoint& accumulated_offset) const {
  if (!ScrollsOverflow())
    return false;

  return Layer()->GetScrollableArea()->HitTestOverflowControls(
      result, RoundedIntPoint(location_in_container -
                              ToLayoutSize(accumulated_offset)));
}

bool LayoutBlock::HitTestOverflowControl(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& adjusted_location) {
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      IsPointInOverflowControl(result, location_in_container.Point(),
                               adjusted_location)) {
    UpdateHitTestResult(result, location_in_container.Point() -
                                    ToLayoutSize(adjusted_location));
    // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
    if (result.AddNodeToListBasedTestResult(
            NodeForHitTest(), location_in_container) == kStopHitTesting)
      return true;
  }
  return false;
}

bool LayoutBlock::HitTestChildren(HitTestResult& result,
                                  const HitTestLocation& location_in_container,
                                  const LayoutPoint& accumulated_offset,
                                  HitTestAction hit_test_action) {
  DCHECK(!ChildrenInline());
  LayoutPoint scrolled_offset(HasOverflowClip()
                                  ? accumulated_offset - ScrolledContentOffset()
                                  : accumulated_offset);
  HitTestAction child_hit_test = hit_test_action;
  if (hit_test_action == kHitTestChildBlockBackgrounds)
    child_hit_test = kHitTestChildBlockBackground;
  for (LayoutBox* child = LastChildBox(); child;
       child = child->PreviousSiblingBox()) {
    LayoutPoint child_point =
        FlipForWritingModeForChild(child, scrolled_offset);
    if (!child->HasSelfPaintingLayer() && !child->IsFloating() &&
        !child->IsColumnSpanAll() &&
        child->NodeAtPoint(result, location_in_container, child_point,
                           child_hit_test)) {
      UpdateHitTestResult(
          result, FlipForWritingMode(ToLayoutPoint(
                      location_in_container.Point() - accumulated_offset)));
      return true;
    }
  }

  return false;
}

Position LayoutBlock::PositionForBox(InlineBox* box, bool start) const {
  if (!box)
    return Position();

  if (!box->GetLineLayoutItem().NonPseudoNode())
    return Position::EditingPositionOf(
        NonPseudoNode(), start ? CaretMinOffset() : CaretMaxOffset());

  if (!box->IsInlineTextBox())
    return Position::EditingPositionOf(
        box->GetLineLayoutItem().NonPseudoNode(),
        start ? box->GetLineLayoutItem().CaretMinOffset()
              : box->GetLineLayoutItem().CaretMaxOffset());

  InlineTextBox* text_box = ToInlineTextBox(box);
  return Position::EditingPositionOf(
      box->GetLineLayoutItem().NonPseudoNode(),
      start ? text_box->Start() : text_box->Start() + text_box->Len());
}

static inline bool IsEditingBoundary(LayoutObject* ancestor,
                                     LineLayoutBox child) {
  DCHECK(!ancestor || ancestor->NonPseudoNode());
  DCHECK(child);
  DCHECK(child.NonPseudoNode());
  return !ancestor || !ancestor->Parent() ||
         (ancestor->HasLayer() && ancestor->Parent()->IsLayoutView()) ||
         HasEditableStyle(*ancestor->NonPseudoNode()) ==
             HasEditableStyle(*child.NonPseudoNode());
}

// FIXME: This function should go on LayoutObject.
// Then all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
PositionWithAffinity LayoutBlock::PositionForPointRespectingEditingBoundaries(
    LineLayoutBox child,
    const LayoutPoint& point_in_parent_coordinates) {
  LayoutPoint child_location = child.Location();
  if (child.IsInFlowPositioned())
    child_location += child.OffsetForInFlowPosition();

  // FIXME: This is wrong if the child's writing-mode is different from the
  // parent's.
  LayoutPoint point_in_child_coordinates(
      ToLayoutPoint(point_in_parent_coordinates - child_location));

  // If this is an anonymous layoutObject, we just recur normally
  Node* child_node = child.NonPseudoNode();
  if (!child_node)
    return child.PositionForPoint(point_in_child_coordinates);

  // Otherwise, first make sure that the editability of the parent and child
  // agree. If they don't agree, then we return a visible position just before
  // or after the child
  LayoutObject* ancestor = this;
  while (ancestor && !ancestor->NonPseudoNode())
    ancestor = ancestor->Parent();

  // If we can't find an ancestor to check editability on, or editability is
  // unchanged, we recur like normal
  if (IsEditingBoundary(ancestor, child))
    return child.PositionForPoint(point_in_child_coordinates);

  // Otherwise return before or after the child, depending on if the click was
  // to the logical left or logical right of the child
  LayoutUnit child_middle = LogicalWidthForChildSize(child.Size()) / 2;
  LayoutUnit logical_left = IsHorizontalWritingMode()
                                ? point_in_child_coordinates.X()
                                : point_in_child_coordinates.Y();
  if (logical_left < child_middle)
    return ancestor->CreatePositionWithAffinity(child_node->NodeIndex());
  return ancestor->CreatePositionWithAffinity(child_node->NodeIndex() + 1,
                                              TextAffinity::kUpstream);
}

PositionWithAffinity LayoutBlock::PositionForPointIfOutsideAtomicInlineLevel(
    const LayoutPoint& point) {
  DCHECK(IsAtomicInlineLevel());
  // FIXME: This seems wrong when the object's writing-mode doesn't match the
  // line's writing-mode.
  LayoutUnit point_logical_left =
      IsHorizontalWritingMode() ? point.X() : point.Y();
  LayoutUnit point_logical_top =
      IsHorizontalWritingMode() ? point.Y() : point.X();

  if (point_logical_left < 0)
    return CreatePositionWithAffinity(CaretMinOffset());
  if (point_logical_left >= LogicalWidth())
    return CreatePositionWithAffinity(CaretMaxOffset());
  if (point_logical_top < 0)
    return CreatePositionWithAffinity(CaretMinOffset());
  if (point_logical_top >= LogicalHeight())
    return CreatePositionWithAffinity(CaretMaxOffset());
  return PositionWithAffinity();
}

static inline bool IsChildHitTestCandidate(LayoutBox* box) {
  return box->Size().Height() &&
         box->Style()->Visibility() == EVisibility::kVisible &&
         !box->IsFloatingOrOutOfFlowPositioned() && !box->IsLayoutFlowThread();
}

PositionWithAffinity LayoutBlock::PositionForPoint(const LayoutPoint& point) {
  if (IsTable())
    return LayoutBox::PositionForPoint(point);

  if (IsAtomicInlineLevel()) {
    PositionWithAffinity position =
        PositionForPointIfOutsideAtomicInlineLevel(point);
    if (!position.IsNull())
      return position;
  }

  LayoutPoint point_in_contents = point;
  OffsetForContents(point_in_contents);
  LayoutPoint point_in_logical_contents(point_in_contents);
  if (!IsHorizontalWritingMode())
    point_in_logical_contents = point_in_logical_contents.TransposedPoint();

  DCHECK(!ChildrenInline());

  LayoutBox* last_candidate_box = LastChildBox();
  while (last_candidate_box && !IsChildHitTestCandidate(last_candidate_box))
    last_candidate_box = last_candidate_box->PreviousSiblingBox();

  bool blocks_are_flipped = Style()->IsFlippedBlocksWritingMode();
  if (last_candidate_box) {
    if (point_in_logical_contents.Y() >
            LogicalTopForChild(*last_candidate_box) ||
        (!blocks_are_flipped && point_in_logical_contents.Y() ==
                                    LogicalTopForChild(*last_candidate_box)))
      return PositionForPointRespectingEditingBoundaries(
          LineLayoutBox(last_candidate_box), point_in_contents);

    for (LayoutBox* child_box = FirstChildBox(); child_box;
         child_box = child_box->NextSiblingBox()) {
      if (!IsChildHitTestCandidate(child_box))
        continue;
      LayoutUnit child_logical_bottom =
          LogicalTopForChild(*child_box) + LogicalHeightForChild(*child_box);
      // We hit child if our click is above the bottom of its padding box (like
      // IE6/7 and FF3).
      if (point_in_logical_contents.Y() < child_logical_bottom ||
          (blocks_are_flipped &&
           point_in_logical_contents.Y() == child_logical_bottom)) {
        return PositionForPointRespectingEditingBoundaries(
            LineLayoutBox(child_box), point_in_contents);
      }
    }
  }

  // We only get here if there are no hit test candidate children below the
  // click.
  return LayoutBox::PositionForPoint(point);
}

void LayoutBlock::OffsetForContents(LayoutPoint& offset) const {
  offset = FlipForWritingMode(offset);

  if (HasOverflowClip())
    offset += LayoutSize(ScrolledContentOffset());

  offset = FlipForWritingMode(offset);
}

int LayoutBlock::ColumnGap() const {
  if (Style()->HasNormalColumnGap()) {
    // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return Style()->GetFontDescription().ComputedPixelSize();
  }
  return static_cast<int>(Style()->ColumnGap());
}

void LayoutBlock::ScrollbarsChanged(bool horizontal_scrollbar_changed,
                                    bool vertical_scrollbar_changed,
                                    ScrollbarChangeContext context) {
  width_available_to_children_changed_ |= vertical_scrollbar_changed;
  height_available_to_children_changed_ |= horizontal_scrollbar_changed;
}

void LayoutBlock::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  // Size-contained elements don't consider their contents for preferred sizing.
  if (Style()->ContainsSize())
    return;

  if (ChildrenInline()) {
    // FIXME: Remove this const_cast.
    ToLayoutBlockFlow(const_cast<LayoutBlock*>(this))
        ->ComputeInlinePreferredLogicalWidths(min_logical_width,
                                              max_logical_width);
  } else {
    ComputeBlockPreferredLogicalWidths(min_logical_width, max_logical_width);
  }

  max_logical_width = std::max(min_logical_width, max_logical_width);

  if (isHTMLMarqueeElement(GetNode()) &&
      toHTMLMarqueeElement(GetNode())->IsHorizontal())
    min_logical_width = LayoutUnit();

  if (IsTableCell()) {
    Length table_cell_width = ToLayoutTableCell(this)->StyleOrColLogicalWidth();
    if (table_cell_width.IsFixed() && table_cell_width.Value() > 0)
      max_logical_width = std::max(min_logical_width,
                                   AdjustContentBoxLogicalWidthForBoxSizing(
                                       LayoutUnit(table_cell_width.Value())));
  }

  int scrollbar_width = ScrollbarLogicalWidth();
  max_logical_width += scrollbar_width;
  min_logical_width += scrollbar_width;
}

DISABLE_CFI_PERF
void LayoutBlock::ComputePreferredLogicalWidths() {
  DCHECK(PreferredLogicalWidthsDirty());

  min_preferred_logical_width_ = LayoutUnit();
  max_preferred_logical_width_ = LayoutUnit();

  // FIXME: The isFixed() calls here should probably be checking for isSpecified
  // since you should be able to use percentage, calc or viewport relative
  // values for width.
  const ComputedStyle& style_to_use = StyleRef();
  if (!IsTableCell() && style_to_use.LogicalWidth().IsFixed() &&
      style_to_use.LogicalWidth().Value() >= 0 &&
      !(IsDeprecatedFlexItem() && !style_to_use.LogicalWidth().IntValue()))
    min_preferred_logical_width_ = max_preferred_logical_width_ =
        AdjustContentBoxLogicalWidthForBoxSizing(
            LayoutUnit(style_to_use.LogicalWidth().Value()));
  else
    ComputeIntrinsicLogicalWidths(min_preferred_logical_width_,
                                  max_preferred_logical_width_);

  if (style_to_use.LogicalMinWidth().IsFixed() &&
      style_to_use.LogicalMinWidth().Value() > 0) {
    max_preferred_logical_width_ =
        std::max(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.LogicalMinWidth().Value())));
    min_preferred_logical_width_ =
        std::max(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.LogicalMinWidth().Value())));
  }

  if (style_to_use.LogicalMaxWidth().IsFixed()) {
    max_preferred_logical_width_ =
        std::min(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.LogicalMaxWidth().Value())));
    min_preferred_logical_width_ =
        std::min(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.LogicalMaxWidth().Value())));
  }

  // Table layout uses integers, ceil the preferred widths to ensure that they
  // can contain the contents.
  if (IsTableCell()) {
    min_preferred_logical_width_ =
        LayoutUnit(min_preferred_logical_width_.Ceil());
    max_preferred_logical_width_ =
        LayoutUnit(max_preferred_logical_width_.Ceil());
  }

  LayoutUnit border_and_padding = BorderAndPaddingLogicalWidth();
  DCHECK_GE(border_and_padding, LayoutUnit());
  min_preferred_logical_width_ += border_and_padding;
  max_preferred_logical_width_ += border_and_padding;

  ClearPreferredLogicalWidthsDirty();
}

void LayoutBlock::ComputeBlockPreferredLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  const ComputedStyle& style_to_use = StyleRef();
  bool nowrap = style_to_use.WhiteSpace() == EWhiteSpace::kNowrap;

  LayoutObject* child = FirstChild();
  LayoutBlock* containing_block = this->ContainingBlock();
  LayoutUnit float_left_width, float_right_width;
  while (child) {
    // Positioned children don't affect the min/max width. Spanners only affect
    // the min/max width of the multicol container, not the flow thread.
    if (child->IsOutOfFlowPositioned() || child->IsColumnSpanAll()) {
      child = child->NextSibling();
      continue;
    }

    if (child->IsBox() &&
        ToLayoutBox(child)->NeedsPreferredWidthsRecalculation()) {
      // We don't really know whether the containing block of this child did
      // change or is going to change size. However, this is our only
      // opportunity to make sure that it gets its min/max widths calculated.
      child->SetPreferredLogicalWidthsDirty();
    }

    RefPtr<ComputedStyle> child_style = child->MutableStyle();
    if (child->IsFloating() ||
        (child->IsBox() && ToLayoutBox(child)->AvoidsFloats())) {
      LayoutUnit float_total_width = float_left_width + float_right_width;
      if (child_style->Clear() == EClear::kBoth ||
          child_style->Clear() == EClear::kLeft) {
        max_logical_width = std::max(float_total_width, max_logical_width);
        float_left_width = LayoutUnit();
      }
      if (child_style->Clear() == EClear::kBoth ||
          child_style->Clear() == EClear::kRight) {
        max_logical_width = std::max(float_total_width, max_logical_width);
        float_right_width = LayoutUnit();
      }
    }

    // A margin basically has three types: fixed, percentage, and auto
    // (variable).
    // Auto and percentage margins simply become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length start_margin_length = child_style->MarginStartUsing(style_to_use);
    Length end_margin_length = child_style->MarginEndUsing(style_to_use);
    LayoutUnit margin;
    LayoutUnit margin_start;
    LayoutUnit margin_end;
    if (start_margin_length.IsFixed())
      margin_start += start_margin_length.Value();
    if (end_margin_length.IsFixed())
      margin_end += end_margin_length.Value();
    margin = margin_start + margin_end;

    LayoutUnit child_min_preferred_logical_width,
        child_max_preferred_logical_width;
    ComputeChildPreferredLogicalWidths(*child,
                                       child_min_preferred_logical_width,
                                       child_max_preferred_logical_width);

    LayoutUnit w = child_min_preferred_logical_width + margin;
    min_logical_width = std::max(w, min_logical_width);

    // IE ignores tables for calculation of nowrap. Makes some sense.
    if (nowrap && !child->IsTable())
      max_logical_width = std::max(w, max_logical_width);

    w = child_max_preferred_logical_width + margin;

    if (!child->IsFloating()) {
      if (child->IsBox() && ToLayoutBox(child)->AvoidsFloats()) {
        // Determine a left and right max value based off whether or not the
        // floats can fit in the margins of the object. For negative margins, we
        // will attempt to overlap the float if the negative margin is smaller
        // than the float width.
        bool ltr = containing_block
                       ? containing_block->Style()->IsLeftToRightDirection()
                       : style_to_use.IsLeftToRightDirection();
        LayoutUnit margin_logical_left = ltr ? margin_start : margin_end;
        LayoutUnit margin_logical_right = ltr ? margin_end : margin_start;
        LayoutUnit max_left =
            margin_logical_left > 0
                ? std::max(float_left_width, margin_logical_left)
                : float_left_width + margin_logical_left;
        LayoutUnit max_right =
            margin_logical_right > 0
                ? std::max(float_right_width, margin_logical_right)
                : float_right_width + margin_logical_right;
        w = child_max_preferred_logical_width + max_left + max_right;
        w = std::max(w, float_left_width + float_right_width);
      } else {
        max_logical_width =
            std::max(float_left_width + float_right_width, max_logical_width);
      }
      float_left_width = float_right_width = LayoutUnit();
    }

    if (child->IsFloating()) {
      if (child_style->Floating() == EFloat::kLeft)
        float_left_width += w;
      else
        float_right_width += w;
    } else {
      max_logical_width = std::max(w, max_logical_width);
    }

    child = child->NextSibling();
  }

  // Always make sure these values are non-negative.
  min_logical_width = min_logical_width.ClampNegativeToZero();
  max_logical_width = max_logical_width.ClampNegativeToZero();

  max_logical_width =
      std::max(float_left_width + float_right_width, max_logical_width);
}

DISABLE_CFI_PERF
void LayoutBlock::ComputeChildPreferredLogicalWidths(
    LayoutObject& child,
    LayoutUnit& min_preferred_logical_width,
    LayoutUnit& max_preferred_logical_width) const {
  if (child.IsBox() &&
      child.IsHorizontalWritingMode() != IsHorizontalWritingMode()) {
    // If the child is an orthogonal flow, child's height determines the width,
    // but the height is not available until layout.
    // http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-shrink-to-fit
    if (!child.NeedsLayout()) {
      min_preferred_logical_width = max_preferred_logical_width =
          ToLayoutBox(child).LogicalHeight();
      return;
    }
    min_preferred_logical_width = max_preferred_logical_width =
        ToLayoutBox(child).ComputeLogicalHeightWithoutLayout();
    return;
  }
  min_preferred_logical_width = child.MinPreferredLogicalWidth();
  max_preferred_logical_width = child.MaxPreferredLogicalWidth();

  // For non-replaced blocks if the inline size is min|max-content or a definite
  // size the min|max-content contribution is that size plus border, padding and
  // margin https://drafts.csswg.org/css-sizing/#block-intrinsic
  if (child.IsLayoutBlock()) {
    const Length& computed_inline_size = child.StyleRef().LogicalWidth();
    if (computed_inline_size.IsMaxContent())
      min_preferred_logical_width = max_preferred_logical_width;
    else if (computed_inline_size.IsMinContent())
      max_preferred_logical_width = min_preferred_logical_width;
  }
}

bool LayoutBlock::HasLineIfEmpty() const {
  if (!GetNode())
    return false;

  if (IsRootEditableElement(*GetNode()))
    return true;

  if (GetNode()->IsShadowRoot() &&
      isHTMLInputElement(ToShadowRoot(GetNode())->host()))
    return true;

  return false;
}

LayoutUnit LayoutBlock::LineHeight(bool first_line,
                                   LineDirectionMode direction,
                                   LinePositionMode line_position_mode) const {
  // Inline blocks are replaced elements. Otherwise, just pass off to
  // the base class.  If we're being queried as though we're the root line
  // box, then the fact that we're an inline-block is irrelevant, and we behave
  // just like a block.
  if (IsAtomicInlineLevel() && line_position_mode == kPositionOnContainingLine)
    return LayoutBox::LineHeight(first_line, direction, line_position_mode);

  const ComputedStyle& style = StyleRef(
      first_line && GetDocument().GetStyleEngine().UsesFirstLineRules());
  return LayoutUnit(style.ComputedLineHeight());
}

int LayoutBlock::BeforeMarginInLineDirection(
    LineDirectionMode direction) const {
  // InlineFlowBox::placeBoxesInBlockDirection will flip lines in
  // case of verticalLR mode, so we can assume verticalRL for now.
  return (direction == kHorizontalLine ? MarginTop() : MarginRight()).ToInt();
}

int LayoutBlock::BaselinePosition(FontBaseline baseline_type,
                                  bool first_line,
                                  LineDirectionMode direction,
                                  LinePositionMode line_position_mode) const {
  // Inline blocks are replaced elements. Otherwise, just pass off to
  // the base class.  If we're being queried as though we're the root line
  // box, then the fact that we're an inline-block is irrelevant, and we behave
  // just like a block.
  if (IsInline() && line_position_mode == kPositionOnContainingLine) {
    // For "leaf" theme objects, let the theme decide what the baseline position
    // is.
    // FIXME: Might be better to have a custom CSS property instead, so that if
    //        the theme is turned off, checkboxes/radios will still have decent
    //        baselines.
    // FIXME: Need to patch form controls to deal with vertical lines.
    if (Style()->HasAppearance() &&
        !LayoutTheme::GetTheme().IsControlContainer(Style()->Appearance()))
      return LayoutTheme::GetTheme().BaselinePosition(this);

    int baseline_pos = (IsWritingModeRoot() && !IsRubyRun())
                           ? -1
                           : InlineBlockBaseline(direction);

    if (IsDeprecatedFlexibleBox()) {
      // Historically, we did this check for all baselines. But we can't
      // remove this code from deprecated flexbox, because it effectively
      // breaks -webkit-line-clamp, which is used in the wild -- we would
      // calculate the baseline as if -webkit-line-clamp wasn't used.
      // For simplicity, we use this for all uses of deprecated flexbox.
      LayoutUnit bottom_of_content =
          direction == kHorizontalLine
              ? Size().Height() - BorderBottom() - PaddingBottom() -
                    HorizontalScrollbarHeight()
              : Size().Width() - BorderLeft() - PaddingLeft() -
                    VerticalScrollbarWidth();
      if (baseline_pos > bottom_of_content)
        baseline_pos = -1;
    }
    if (baseline_pos != -1)
      return BeforeMarginInLineDirection(direction) + baseline_pos;

    return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                       line_position_mode);
  }

  // If we're not replaced, we'll only get called with
  // PositionOfInteriorLineBoxes.
  // Note that inline-block counts as replaced here.
  DCHECK_EQ(line_position_mode, kPositionOfInteriorLineBoxes);

  const SimpleFontData* font_data = Style(first_line)->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return -1;

  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  return (font_metrics.Ascent(baseline_type) +
          (LineHeight(first_line, direction, line_position_mode) -
           font_metrics.Height()) /
              2)
      .ToInt();
}

LayoutUnit LayoutBlock::MinLineHeightForReplacedObject(
    bool is_first_line,
    LayoutUnit replaced_height) const {
  if (!GetDocument().InNoQuirksMode() && replaced_height)
    return replaced_height;

  return std::max<LayoutUnit>(
      replaced_height,
      LineHeight(is_first_line,
                 IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine,
                 kPositionOfInteriorLineBoxes));
}

// TODO(mstensho): Figure out if all of this baseline code is needed here, or if
// it should be moved down to LayoutBlockFlow. LayoutDeprecatedFlexibleBox and
// LayoutGrid lack baseline calculation overrides, so the code is here just for
// them. Just walking the block children in logical order seems rather wrong for
// those two layout modes, though.

int LayoutBlock::FirstLineBoxBaseline() const {
  DCHECK(!ChildrenInline());
  if (IsWritingModeRoot() && !IsRubyRun())
    return -1;

  for (LayoutBox* curr = FirstChildBox(); curr; curr = curr->NextSiblingBox()) {
    if (!curr->IsFloatingOrOutOfFlowPositioned()) {
      int result = curr->FirstLineBoxBaseline();
      if (result != -1)
        return (curr->LogicalTop() + result)
            .ToInt();  // Translate to our coordinate space.
    }
  }
  return -1;
}

bool LayoutBlock::UseLogicalBottomMarginEdgeForInlineBlockBaseline() const {
  // CSS2.1 states that the baseline of an 'inline-block' is:
  // the baseline of the last line box in the normal flow, unless it has
  // either no in-flow line boxes or if its 'overflow' property has a computed
  // value other than 'visible', in which case the baseline is the bottom
  // margin edge.
  // We likewise avoid using the last line box in the case of size containment,
  // where the block's contents shouldn't be considered when laying out its
  // ancestors or siblings.
  return (!Style()->IsOverflowVisible() &&
          !ShouldIgnoreOverflowPropertyForInlineBlockBaseline()) ||
         Style()->ContainsSize();
}

int LayoutBlock::InlineBlockBaseline(LineDirectionMode line_direction) const {
  DCHECK(!ChildrenInline());
  if (UseLogicalBottomMarginEdgeForInlineBlockBaseline()) {
    // We are not calling LayoutBox::baselinePosition here because the caller
    // should add the margin-top/margin-right, not us.
    return (line_direction == kHorizontalLine ? Size().Height() + MarginBottom()
                                              : Size().Width() + MarginLeft())
        .ToInt();
  }

  if (IsWritingModeRoot() && !IsRubyRun())
    return -1;

  bool have_normal_flow_child = false;
  for (LayoutBox* curr = LastChildBox(); curr;
       curr = curr->PreviousSiblingBox()) {
    if (!curr->IsFloatingOrOutOfFlowPositioned()) {
      have_normal_flow_child = true;
      int result = curr->InlineBlockBaseline(line_direction);
      if (result != -1)
        return (curr->LogicalTop() + result)
            .ToInt();  // Translate to our coordinate space.
    }
  }
  const SimpleFontData* font_data = FirstLineStyle()->GetFont().PrimaryFont();
  if (font_data && !have_normal_flow_child && HasLineIfEmpty()) {
    const FontMetrics& font_metrics = font_data->GetFontMetrics();
    return (font_metrics.Ascent() +
            (LineHeight(true, line_direction, kPositionOfInteriorLineBoxes) -
             font_metrics.Height()) /
                2 +
            (line_direction == kHorizontalLine
                 ? BorderTop() + PaddingTop()
                 : BorderRight() + PaddingRight()))
        .ToInt();
  }
  return -1;
}

const LayoutBlock* LayoutBlock::EnclosingFirstLineStyleBlock() const {
  const LayoutBlock* first_line_block = this;
  bool has_pseudo = false;
  while (true) {
    has_pseudo = first_line_block->Style()->HasPseudoStyle(kPseudoIdFirstLine);
    if (has_pseudo)
      break;
    LayoutObject* parent_block = first_line_block->Parent();
    if (first_line_block->IsAtomicInlineLevel() ||
        first_line_block->IsFloatingOrOutOfFlowPositioned() || !parent_block ||
        !parent_block->BehavesLikeBlockContainer())
      break;
    SECURITY_DCHECK(parent_block->IsLayoutBlock());
    if (ToLayoutBlock(parent_block)->FirstChild() != first_line_block)
      break;
    first_line_block = ToLayoutBlock(parent_block);
  }

  if (!has_pseudo)
    return nullptr;

  return first_line_block;
}

LayoutBlockFlow* LayoutBlock::NearestInnerBlockWithFirstLine() {
  if (ChildrenInline())
    return ToLayoutBlockFlow(this);
  for (LayoutObject* child = FirstChild();
       child && !child->IsFloatingOrOutOfFlowPositioned() &&
       child->IsLayoutBlockFlow();
       child = ToLayoutBlock(child)->FirstChild()) {
    if (child->ChildrenInline())
      return ToLayoutBlockFlow(child);
  }
  return nullptr;
}

void LayoutBlock::UpdateHitTestResult(HitTestResult& result,
                                      const LayoutPoint& point) {
  if (result.InnerNode())
    return;

  if (Node* n = NodeForHitTest())
    result.SetNodeAndPosition(n, point);
}

// An inline-block uses its inlineBox as the inlineBoxWrapper,
// so the firstChild() is nullptr if the only child is an empty inline-block.
inline bool LayoutBlock::IsInlineBoxWrapperActuallyChild() const {
  return IsInlineBlockOrInlineTable() && !Size().IsEmpty() && GetNode() &&
         EditingIgnoresContent(*GetNode());
}

bool LayoutBlock::ShouldPaintCursorCaret() const {
  return GetFrame()->Selection().ShouldPaintCaret(*this);
}

bool LayoutBlock::ShouldPaintDragCaret() const {
  return GetFrame()->GetPage()->GetDragCaret().ShouldPaintCaret(*this);
}

LayoutRect LayoutBlock::LocalCaretRect(InlineBox* inline_box,
                                       int caret_offset,
                                       LayoutUnit* extra_width_to_end_of_line) {
  // Do the normal calculation in most cases.
  if ((FirstChild() && !FirstChild()->IsPseudoElement()) ||
      IsInlineBoxWrapperActuallyChild())
    return LayoutBox::LocalCaretRect(inline_box, caret_offset,
                                     extra_width_to_end_of_line);

  LayoutRect caret_rect =
      LocalCaretRectForEmptyElement(Size().Width(), TextIndentOffset());

  if (extra_width_to_end_of_line)
    *extra_width_to_end_of_line = Size().Width() - caret_rect.MaxX();

  return caret_rect;
}

void LayoutBlock::AddOutlineRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    IncludeBlockVisualOverflowOrNot include_block_overflows) const {
  if (!IsAnonymous())  // For anonymous blocks, the children add outline rects.
    rects.push_back(LayoutRect(additional_offset, Size()));

  if (include_block_overflows == kIncludeBlockVisualOverflow &&
      !HasOverflowClip() && !HasControlClip()) {
    AddOutlineRectsForNormalChildren(rects, additional_offset,
                                     include_block_overflows);
    if (TrackedLayoutBoxListHashSet* positioned_objects =
            this->PositionedObjects()) {
      for (auto* box : *positioned_objects)
        AddOutlineRectsForDescendant(*box, rects, additional_offset,
                                     include_block_overflows);
    }
  }
}

LayoutBox* LayoutBlock::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  return CreateAnonymousWithParentAndDisplay(parent, Style()->Display());
}

void LayoutBlock::PaginatedContentWasLaidOut(
    LayoutUnit logical_bottom_offset_after_pagination) {
  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock())
    flow_thread->ContentWasLaidOut(OffsetFromLogicalTopOfFirstPage() +
                                   logical_bottom_offset_after_pagination);
}

LayoutUnit LayoutBlock::CollapsedMarginBeforeForChild(
    const LayoutBox& child) const {
  // If the child has the same directionality as we do, then we can just return
  // its collapsed margin.
  if (!child.IsWritingModeRoot())
    return child.CollapsedMarginBefore();

  // The child has a different directionality.  If the child is parallel, then
  // it's just flipped relative to us.  We can use the collapsed margin for the
  // opposite edge.
  if (child.IsHorizontalWritingMode() == IsHorizontalWritingMode())
    return child.CollapsedMarginAfter();

  // The child is perpendicular to us, which means its margins don't collapse
  // but are on the "logical left/right" sides of the child box. We can just
  // return the raw margin in this case.
  return MarginBeforeForChild(child);
}

LayoutUnit LayoutBlock::CollapsedMarginAfterForChild(
    const LayoutBox& child) const {
  // If the child has the same directionality as we do, then we can just return
  // its collapsed margin.
  if (!child.IsWritingModeRoot())
    return child.CollapsedMarginAfter();

  // The child has a different directionality.  If the child is parallel, then
  // it's just flipped relative to us.  We can use the collapsed margin for the
  // opposite edge.
  if (child.IsHorizontalWritingMode() == IsHorizontalWritingMode())
    return child.CollapsedMarginBefore();

  // The child is perpendicular to us, which means its margins don't collapse
  // but are on the "logical left/right" side of the child box. We can just
  // return the raw margin in this case.
  return MarginAfterForChild(child);
}

bool LayoutBlock::HasMarginBeforeQuirk(const LayoutBox* child) const {
  // If the child has the same directionality as we do, then we can just return
  // its margin quirk.
  if (!child->IsWritingModeRoot())
    return child->IsLayoutBlock() ? ToLayoutBlock(child)->HasMarginBeforeQuirk()
                                  : child->Style()->HasMarginBeforeQuirk();

  // The child has a different directionality. If the child is parallel, then
  // it's just flipped relative to us. We can use the opposite edge.
  if (child->IsHorizontalWritingMode() == IsHorizontalWritingMode())
    return child->IsLayoutBlock() ? ToLayoutBlock(child)->HasMarginAfterQuirk()
                                  : child->Style()->HasMarginAfterQuirk();

  // The child is perpendicular to us and box sides are never quirky in
  // html.css, and we don't really care about whether or not authors specified
  // quirky ems, since they're an implementation detail.
  return false;
}

bool LayoutBlock::HasMarginAfterQuirk(const LayoutBox* child) const {
  // If the child has the same directionality as we do, then we can just return
  // its margin quirk.
  if (!child->IsWritingModeRoot())
    return child->IsLayoutBlock() ? ToLayoutBlock(child)->HasMarginAfterQuirk()
                                  : child->Style()->HasMarginAfterQuirk();

  // The child has a different directionality. If the child is parallel, then
  // it's just flipped relative to us. We can use the opposite edge.
  if (child->IsHorizontalWritingMode() == IsHorizontalWritingMode())
    return child->IsLayoutBlock() ? ToLayoutBlock(child)->HasMarginBeforeQuirk()
                                  : child->Style()->HasMarginBeforeQuirk();

  // The child is perpendicular to us and box sides are never quirky in
  // html.css, and we don't really care about whether or not authors specified
  // quirky ems, since they're an implementation detail.
  return false;
}

const char* LayoutBlock::GetName() const {
  NOTREACHED();
  return "LayoutBlock";
}

LayoutBlock* LayoutBlock::CreateAnonymousWithParentAndDisplay(
    const LayoutObject* parent,
    EDisplay display) {
  // FIXME: Do we need to convert all our inline displays to block-type in the
  // anonymous logic ?
  EDisplay new_display;
  LayoutBlock* new_box = nullptr;
  if (display == EDisplay::kFlex || display == EDisplay::kInlineFlex) {
    new_box = LayoutFlexibleBox::CreateAnonymous(&parent->GetDocument());
    new_display = EDisplay::kFlex;
  } else {
    new_box = LayoutBlockFlow::CreateAnonymous(&parent->GetDocument());
    new_display = EDisplay::kBlock;
  }

  RefPtr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent->StyleRef(),
                                                     new_display);
  parent->UpdateAnonymousChildStyle(*new_box, *new_style);
  new_box->SetStyle(std::move(new_style));
  return new_box;
}

bool LayoutBlock::RecalcNormalFlowChildOverflowIfNeeded(
    LayoutObject* layout_object) {
  if (layout_object->IsOutOfFlowPositioned() ||
      !layout_object->NeedsOverflowRecalcAfterStyleChange())
    return false;

  DCHECK(layout_object->IsLayoutBlock());
  return ToLayoutBlock(layout_object)->RecalcOverflowAfterStyleChange();
}

bool LayoutBlock::RecalcChildOverflowAfterStyleChange() {
  DCHECK(ChildNeedsOverflowRecalcAfterStyleChange());
  ClearChildNeedsOverflowRecalcAfterStyleChange();

  bool children_overflow_changed = false;

  if (ChildrenInline()) {
    SECURITY_DCHECK(IsLayoutBlockFlow());
    children_overflow_changed =
        ToLayoutBlockFlow(this)->RecalcInlineChildrenOverflowAfterStyleChange();
  } else {
    for (LayoutBox* box = FirstChildBox(); box; box = box->NextSiblingBox()) {
      if (RecalcNormalFlowChildOverflowIfNeeded(box))
        children_overflow_changed = true;
    }
  }

  return RecalcPositionedDescendantsOverflowAfterStyleChange() ||
         children_overflow_changed;
}

bool LayoutBlock::RecalcPositionedDescendantsOverflowAfterStyleChange() {
  bool children_overflow_changed = false;

  TrackedLayoutBoxListHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return children_overflow_changed;

  for (auto* box : *positioned_descendants) {
    if (!box->NeedsOverflowRecalcAfterStyleChange())
      continue;
    LayoutBlock* block = ToLayoutBlock(box);
    if (!block->RecalcOverflowAfterStyleChange() ||
        box->Style()->GetPosition() == EPosition::kFixed)
      continue;

    children_overflow_changed = true;
  }
  return children_overflow_changed;
}

bool LayoutBlock::RecalcOverflowAfterStyleChange() {
  DCHECK(NeedsOverflowRecalcAfterStyleChange());

  bool children_overflow_changed = false;
  if (ChildNeedsOverflowRecalcAfterStyleChange())
    children_overflow_changed = RecalcChildOverflowAfterStyleChange();

  bool self_needs_overflow_recalc = SelfNeedsOverflowRecalcAfterStyleChange();
  if (!self_needs_overflow_recalc && !children_overflow_changed)
    return false;

  ClearSelfNeedsOverflowRecalcAfterStyleChange();
  // If the current block needs layout, overflow will be recalculated during
  // layout time anyway. We can safely exit here.
  if (NeedsLayout())
    return false;

  LayoutUnit old_client_after_edge = HasOverflowModel()
                                         ? overflow_->LayoutClientAfterEdge()
                                         : ClientLogicalBottom();
  ComputeOverflow(old_client_after_edge, true);

  if (HasOverflowClip())
    Layer()->GetScrollableArea()->UpdateAfterOverflowRecalc();

  return !HasOverflowClip() || self_needs_overflow_recalc;
}

// Called when a positioned object moves but doesn't necessarily change size.
// A simplified layout is attempted that just updates the object's position.
// If the size does change, the object remains dirty.
bool LayoutBlock::TryLayoutDoingPositionedMovementOnly() {
  LayoutUnit old_width = LogicalWidth();
  LogicalExtentComputedValues computed_values;
  LogicalExtentAfterUpdatingLogicalWidth(LogicalTop(), computed_values);
  // If we shrink to fit our width may have changed, so we still need full
  // layout.
  if (old_width != computed_values.extent_)
    return false;
  SetLogicalWidth(computed_values.extent_);
  SetLogicalLeft(computed_values.position_);
  SetMarginStart(computed_values.margins_.start_);
  SetMarginEnd(computed_values.margins_.end_);

  LayoutUnit old_height = LogicalHeight();
  LayoutUnit old_intrinsic_content_logical_height =
      IntrinsicContentLogicalHeight();

  SetIntrinsicContentLogicalHeight(ContentLogicalHeight());
  ComputeLogicalHeight(old_height, LogicalTop(), computed_values);

  if (old_height != computed_values.extent_ &&
      (HasPercentHeightDescendants() || IsFlexibleBox())) {
    SetIntrinsicContentLogicalHeight(old_intrinsic_content_logical_height);
    return false;
  }

  SetLogicalHeight(computed_values.extent_);
  SetLogicalTop(computed_values.position_);
  SetMarginBefore(computed_values.margins_.before_);
  SetMarginAfter(computed_values.margins_.after_);

  return true;
}

#if DCHECK_IS_ON()
void LayoutBlock::CheckPositionedObjectsNeedLayout() {
  if (!g_positioned_descendants_map)
    return;

  if (TrackedLayoutBoxListHashSet* positioned_descendant_set =
          PositionedObjects()) {
    TrackedLayoutBoxListHashSet::const_iterator end =
        positioned_descendant_set->end();
    for (TrackedLayoutBoxListHashSet::const_iterator it =
             positioned_descendant_set->begin();
         it != end; ++it) {
      LayoutBox* curr_box = *it;
      DCHECK(!curr_box->NeedsLayout());
    }
  }
}

#endif

LayoutUnit LayoutBlock::AvailableLogicalHeightForPercentageComputation() const {
  LayoutUnit available_height(-1);

  // For anonymous blocks that are skipped during percentage height calculation,
  // we consider them to have an indefinite height.
  if (SkipContainingBlockForPercentHeightCalculation(this))
    return available_height;

  const ComputedStyle& style = StyleRef();

  // A positioned element that specified both top/bottom or that specifies
  // height should be treated as though it has a height explicitly specified
  // that can be used for any percentage computations.
  bool is_out_of_flow_positioned_with_specified_height =
      IsOutOfFlowPositioned() &&
      (!style.LogicalHeight().IsAuto() ||
       (!style.LogicalTop().IsAuto() && !style.LogicalBottom().IsAuto()));

  LayoutUnit stretched_flex_height(-1);
  if (IsFlexItem())
    stretched_flex_height =
        ToLayoutFlexibleBox(Parent())
            ->ChildLogicalHeightForPercentageResolution(*this);

  if (stretched_flex_height != LayoutUnit(-1)) {
    available_height = stretched_flex_height;
  } else if (IsGridItem() && HasOverrideLogicalContentHeight()) {
    available_height = OverrideLogicalContentHeight();
  } else if (style.LogicalHeight().IsFixed()) {
    LayoutUnit content_box_height = AdjustContentBoxLogicalHeightForBoxSizing(
        style.LogicalHeight().Value());
    available_height = std::max(
        LayoutUnit(),
        ConstrainContentBoxLogicalHeightByMinMax(
            content_box_height - ScrollbarLogicalHeight(), LayoutUnit(-1)));
  } else if (style.LogicalHeight().IsPercentOrCalc() &&
             !is_out_of_flow_positioned_with_specified_height) {
    LayoutUnit height_with_scrollbar =
        ComputePercentageLogicalHeight(style.LogicalHeight());
    if (height_with_scrollbar != -1) {
      LayoutUnit content_box_height_with_scrollbar =
          AdjustContentBoxLogicalHeightForBoxSizing(height_with_scrollbar);
      // We need to adjust for min/max height because this method does not
      // handle the min/max of the current block, its caller does. So the
      // return value from the recursive call will not have been adjusted
      // yet.
      LayoutUnit content_box_height = ConstrainContentBoxLogicalHeightByMinMax(
          content_box_height_with_scrollbar - ScrollbarLogicalHeight(),
          LayoutUnit(-1));
      available_height = std::max(LayoutUnit(), content_box_height);
    }
  } else if (is_out_of_flow_positioned_with_specified_height) {
    // Don't allow this to affect the block' size() member variable, since this
    // can get called while the block is still laying out its kids.
    LogicalExtentComputedValues computed_values;
    ComputeLogicalHeight(LogicalHeight(), LayoutUnit(), computed_values);
    available_height = computed_values.extent_ -
                       BorderAndPaddingLogicalHeight() -
                       ScrollbarLogicalHeight();
  } else if (IsLayoutView()) {
    available_height = View()->ViewLogicalHeightForPercentages();
  }

  return available_height;
}

bool LayoutBlock::HasDefiniteLogicalHeight() const {
  return AvailableLogicalHeightForPercentageComputation() != LayoutUnit(-1);
}

bool LayoutBlock::NeedsPreferredWidthsRecalculation() const {
  return (HasRelativeLogicalHeight() && Style()->LogicalWidth().IsAuto()) ||
         LayoutBox::NeedsPreferredWidthsRecalculation();
}

}  // namespace blink

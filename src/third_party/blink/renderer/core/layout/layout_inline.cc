/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
 *               All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_inline.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/inline_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/region.h"

namespace blink {

namespace {

// TODO(layout-dev): Once we generate fragment for all inline element, we should
// use |LayoutObject::EnclosingBlockFlowFragment()|.
const NGPhysicalBoxFragment* ContainingBlockFlowFragmentOf(
    const LayoutInline& node) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  return node.ContainingBlockFlowFragment();
}

// TODO(xiaochengh): Deduplicate with a similar function in ng_paint_fragment.cc
// ::before, ::after and ::first-letter can be hit test targets.
bool CanBeHitTestTargetPseudoNodeStyle(const ComputedStyle& style) {
  switch (style.StyleType()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdFirstLetter:
      return true;
    default:
      return false;
  }
}

}  // anonymous namespace

struct SameSizeAsLayoutInline : public LayoutBoxModelObject {
  ~SameSizeAsLayoutInline() override = default;
  LayoutObjectChildList children_;
  LineBoxList line_boxes_;
};

static_assert(sizeof(LayoutInline) == sizeof(SameSizeAsLayoutInline),
              "LayoutInline should stay small");

LayoutInline::LayoutInline(Element* element)
    : LayoutBoxModelObject(element), line_boxes_() {
  SetChildrenInline(true);
}

LayoutInline::~LayoutInline() {
#if DCHECK_IS_ON()
  if (!IsInLayoutNGInlineFormattingContext())
    line_boxes_.AssertIsEmpty();
#endif
}

LayoutInline* LayoutInline::CreateAnonymous(Document* document) {
  LayoutInline* layout_inline = new LayoutInline(nullptr);
  layout_inline->SetDocumentForAnonymous(document);
  return layout_inline;
}

void LayoutInline::WillBeDestroyed() {
  // Make sure to destroy anonymous children first while they are still
  // connected to the rest of the tree, so that they will properly dirty line
  // boxes that they are removed from. Effects that do :before/:after only on
  // hover could crash otherwise.
  Children()->DestroyLeftoverChildren();

  // Destroy our continuation before anything other than anonymous children.
  // The reason we don't destroy it before anonymous children is that they may
  // have continuations of their own that are anonymous children of our
  // continuation.
  LayoutBoxModelObject* continuation = Continuation();
  if (continuation) {
    continuation->Destroy();
    SetContinuation(nullptr);
  }

  if (!DocumentBeingDestroyed()) {
    if (FirstLineBox()) {
      // If line boxes are contained inside a root, that means we're an inline.
      // In that case, we need to remove all the line boxes so that the parent
      // lines aren't pointing to deleted children. If the first line box does
      // not have a parent that means they are either already disconnected or
      // root lines that can just be destroyed without disconnecting.
      if (FirstLineBox()->Parent()) {
        for (InlineFlowBox* box : *LineBoxes())
          box->Remove();
      }
    } else {
      if (Parent())
        Parent()->DirtyLinesFromChangedChild(this);
      if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
        if (NGPaintFragment* first_inline_fragment = FirstInlineFragment())
          first_inline_fragment->LayoutObjectWillBeDestroyed();
      } else if (FirstInlineFragmentItemIndex()) {
        NGFragmentItems::LayoutObjectWillBeDestroyed(*this);
        ClearFirstInlineFragmentItemIndex();
      }
    }
  }

  DeleteLineBoxes();

  LayoutBoxModelObject::WillBeDestroyed();
}

void LayoutInline::DeleteLineBoxes() {
  if (!IsInLayoutNGInlineFormattingContext())
    MutableLineBoxes()->DeleteLineBoxes();
}

void LayoutInline::SetFirstInlineFragment(NGPaintFragment* fragment) {
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  DCHECK(!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  first_paint_fragment_ = fragment;
}

void LayoutInline::ClearFirstInlineFragmentItemIndex() {
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  first_fragment_item_index_ = 0u;
}

void LayoutInline::SetFirstInlineFragmentItemIndex(wtf_size_t index) {
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK_NE(index, 0u);
  first_fragment_item_index_ = index;
}

bool LayoutInline::HasInlineFragments() const {
  if (!IsInLayoutNGInlineFormattingContext())
    return FirstLineBox();
  if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return first_paint_fragment_;
  return first_fragment_item_index_;
}

void LayoutInline::InLayoutNGInlineFormattingContextWillChange(bool new_value) {
  if (IsInLayoutNGInlineFormattingContext()) {
    if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
      SetFirstInlineFragment(nullptr);
    } else {
      ClearFirstInlineFragmentItemIndex();
    }
  } else {
    DeleteLineBoxes();
  }

  // Because |first_paint_fragment_| and |line_boxes_| are union, when one is
  // deleted, the other should be initialized to nullptr.
  DCHECK(new_value ? (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()
                          ? !first_fragment_item_index_
                          : !first_paint_fragment_)
                   : !line_boxes_.First());
}

LayoutInline* LayoutInline::InlineElementContinuation() const {
  LayoutBoxModelObject* continuation = Continuation();
  if (!continuation || continuation->IsInline())
    return ToLayoutInline(continuation);
  return To<LayoutBlockFlow>(continuation)->InlineElementContinuation();
}

void LayoutInline::UpdateFromStyle() {
  LayoutBoxModelObject::UpdateFromStyle();

  // FIXME: Is this still needed. Was needed for run-ins, since run-in is
  // considered a block display type.
  SetInline(true);

  // FIXME: Support transforms and reflections on inline flows someday.
  SetHasTransformRelatedProperty(false);
  SetHasReflection(false);
}

static LayoutObject* InFlowPositionedInlineAncestor(LayoutObject* p) {
  while (p && p->IsLayoutInline()) {
    if (p->IsInFlowPositioned())
      return p;
    p = p->Parent();
  }
  return nullptr;
}

static void UpdateInFlowPositionOfAnonymousBlockContinuations(
    LayoutObject* block,
    const ComputedStyle& new_style,
    const ComputedStyle& old_style,
    LayoutObject* containing_block_of_end_of_continuation) {
  for (; block && block != containing_block_of_end_of_continuation &&
         block->IsAnonymousBlock();
       block = block->NextSibling()) {
    auto* block_flow = To<LayoutBlockFlow>(block);
    if (!block_flow->IsAnonymousBlockContinuation())
      continue;

    // If we are no longer in-flow positioned but our descendant block(s) still
    // have an in-flow positioned ancestor then their containing anonymous block
    // should keep its in-flow positioning.
    if (old_style.HasInFlowPosition() &&
        InFlowPositionedInlineAncestor(block_flow->InlineElementContinuation()))
      continue;

    scoped_refptr<ComputedStyle> new_block_style =
        ComputedStyle::Clone(block->StyleRef());
    new_block_style->SetPosition(new_style.GetPosition());
    block->SetStyle(new_block_style);
  }
}

void LayoutInline::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  LayoutBoxModelObject::StyleDidChange(diff, old_style);

  // Ensure that all of the split inlines pick up the new style. We only do this
  // if we're an inline, since we don't want to propagate a block's style to the
  // other inlines. e.g., <font>foo <h4>goo</h4> moo</font>.  The <font> inlines
  // before and after the block share the same style, but the block doesn't need
  // to pass its style on to anyone else.
  const ComputedStyle& new_style = StyleRef();
  if (LayoutInline* continuation = InlineElementContinuation()) {
    bool position_type_changed =
        old_style && (new_style.GetPosition() != old_style->GetPosition()) &&
        (new_style.HasInFlowPosition() || old_style->HasInFlowPosition());

    // An inline that establishes a continuation is normally wrapped inside an
    // anonymous block, of course, but if the continuation chain has been
    // partially torn down (read comment further below), this is not the
    // case. Here we want to find the nearest containing block that's an
    // ancestor of all the continuations.
    const LayoutBlock* boundary = ContainingBlock();
    if (boundary->IsAnonymousBlock())
      boundary = boundary->ContainingBlock();
    LayoutInline* previous_part = this;
    LayoutInline* end_of_continuation = nullptr;
    bool needs_anon_block_position_update = false;
    for (LayoutInline* curr_cont = continuation; curr_cont;
         curr_cont = curr_cont->InlineElementContinuation()) {
      if (position_type_changed && !needs_anon_block_position_update) {
        // When we have a continuation chain, and the block child that was the
        // reason for creating that in the first place is removed, we don't
        // clean up the continuation chain. Previously split inlines should
        // ideally be joined when this happens, but we don't do that, but rather
        // leave the mess around. We'll end up with LayoutInline siblings or
        // cousins for the same Node (that form a bogus continuation chain),
        // without any blocks in-between. Here we check that we're NOT in such a
        // situation, and that the Node actually forms a real continuation
        // chain. This matters when it comes to marking the anonymous
        // container(s) of block children as relatively positioned (further
        // below). We should only do that if the Node is an ancestor of the
        // blocks. Otherwise out-of-flow positioned descendants will use the
        // wrong containing block.
        for (LayoutObject* walker = previous_part->NextInPreOrder(boundary);
             walker;) {
          if (walker == curr_cont)
            break;
          if (walker->IsAnonymousBlock()) {
            needs_anon_block_position_update = true;
            break;
          }
          if (walker->IsLayoutInline())
            walker = walker->NextInPreOrder(boundary);
          else
            walker = walker->NextInPreOrderAfterChildren(boundary);
        }
      }
      previous_part = curr_cont;

      LayoutBoxModelObject* next_cont = curr_cont->Continuation();
      curr_cont->SetContinuation(nullptr);
      curr_cont->SetStyle(Style());
      curr_cont->SetContinuation(next_cont);
      end_of_continuation = curr_cont;
    }

    if (needs_anon_block_position_update) {
      DCHECK(old_style);
      DCHECK(end_of_continuation);
      LayoutObject* block = ContainingBlock()->NextSibling();
      // If an inline's in-flow positioning has changed then any descendant
      // blocks will need to change their styles accordingly.
      if (block && block->IsAnonymousBlock()) {
        UpdateInFlowPositionOfAnonymousBlockContinuations(
            block, new_style, *old_style,
            end_of_continuation->ContainingBlock());
      }
    }
  }

  if (!IsInLayoutNGInlineFormattingContext()) {
    if (!AlwaysCreateLineBoxes()) {
      bool always_create_line_boxes_new =
          HasSelfPaintingLayer() || HasBoxDecorationBackground() ||
          new_style.MayHavePadding() || new_style.MayHaveMargin() ||
          new_style.HasOutline();
      if (old_style && always_create_line_boxes_new) {
        DirtyLineBoxes(false);
        SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kStyleChange);
      }
      SetAlwaysCreateLineBoxes(always_create_line_boxes_new);
    }
  } else {
    if (!ShouldCreateBoxFragment()) {
      UpdateShouldCreateBoxFragment();
    }
    if (diff.NeedsCollectInlines()) {
      SetNeedsCollectInlines();
    }
  }

  PropagateStyleToAnonymousChildren();
}

void LayoutInline::UpdateAlwaysCreateLineBoxes(bool full_layout) {
  DCHECK(!IsInLayoutNGInlineFormattingContext());

  // Once we have been tainted once, just assume it will happen again. This way
  // effects like hover highlighting that change the background color will only
  // cause a layout on the first rollover.
  if (AlwaysCreateLineBoxes())
    return;

  const ComputedStyle& parent_style = Parent()->StyleRef();
  LayoutInline* parent_layout_inline =
      Parent()->IsLayoutInline() ? ToLayoutInline(Parent()) : nullptr;
  bool check_fonts = GetDocument().InNoQuirksMode();
  bool always_create_line_boxes_new =
      (parent_layout_inline && parent_layout_inline->AlwaysCreateLineBoxes()) ||
      (parent_layout_inline &&
       parent_style.VerticalAlign() != EVerticalAlign::kBaseline) ||
      StyleRef().VerticalAlign() != EVerticalAlign::kBaseline ||
      StyleRef().GetTextEmphasisMark() != TextEmphasisMark::kNone ||
      (check_fonts &&
       (!StyleRef().HasIdenticalAscentDescentAndLineGap(parent_style) ||
        parent_style.LineHeight() != StyleRef().LineHeight()));

  if (!always_create_line_boxes_new && check_fonts &&
      GetDocument().GetStyleEngine().UsesFirstLineRules()) {
    // Have to check the first line style as well.
    const ComputedStyle& first_line_parent_style = Parent()->StyleRef(true);
    const ComputedStyle& child_style = StyleRef(true);
    always_create_line_boxes_new =
        !first_line_parent_style.HasIdenticalAscentDescentAndLineGap(
            child_style) ||
        child_style.VerticalAlign() != EVerticalAlign::kBaseline ||
        first_line_parent_style.LineHeight() != child_style.LineHeight();
  }

  if (always_create_line_boxes_new) {
    if (!full_layout)
      DirtyLineBoxes(false);
    SetAlwaysCreateLineBoxes();
  }
}

bool LayoutInline::ComputeInitialShouldCreateBoxFragment(
    const ComputedStyle& style) const {
  if (style.HasBoxDecorationBackground() || style.MayHavePadding() ||
      style.MayHaveMargin())
    return true;

  return ComputeIsAbsoluteContainer(&style) ||
         NGOutlineUtils::HasPaintedOutline(style, GetNode()) ||
         CanBeHitTestTargetPseudoNodeStyle(style);
}

bool LayoutInline::ComputeInitialShouldCreateBoxFragment() const {
  const ComputedStyle& style = StyleRef();
  if (HasSelfPaintingLayer() || ComputeInitialShouldCreateBoxFragment(style) ||
      ShouldApplyPaintContainment() || ShouldApplyLayoutContainment())
    return true;

  const ComputedStyle& first_line_style = FirstLineStyleRef();
  if (UNLIKELY(&style != &first_line_style &&
               ComputeInitialShouldCreateBoxFragment(first_line_style)))
    return true;

  return false;
}

void LayoutInline::UpdateShouldCreateBoxFragment() {
  // Once we have been tainted once, just assume it will happen again. This way
  // effects like hover highlighting that change the background color will only
  // cause a layout on the first rollover.
  if (IsInLayoutNGInlineFormattingContext()) {
    if (ShouldCreateBoxFragment())
      return;
  } else {
    SetIsInLayoutNGInlineFormattingContext(true);
    SetShouldCreateBoxFragment(false);
  }

  if (ComputeInitialShouldCreateBoxFragment()) {
    SetShouldCreateBoxFragment();
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }
}

LayoutRect LayoutInline::LocalCaretRect(
    const InlineBox* inline_box,
    int,
    LayoutUnit* extra_width_to_end_of_line) const {
  if (FirstChild()) {
    // This condition is possible if the LayoutInline is at an editing boundary,
    // i.e. the VisiblePosition is:
    //   <LayoutInline editingBoundary=true>|<LayoutText>
    //   </LayoutText></LayoutInline>
    // FIXME: need to figure out how to make this return a valid rect, note that
    // there are no line boxes created in the above case.
    return LayoutRect();
  }

  DCHECK(!inline_box);

  if (extra_width_to_end_of_line)
    *extra_width_to_end_of_line = LayoutUnit();

  LayoutRect caret_rect =
      LocalCaretRectForEmptyElement(BorderAndPaddingWidth(), LayoutUnit());

  if (InlineBox* first_box = FirstLineBox())
    caret_rect.MoveBy(first_box->Location());

  return caret_rect;
}

void LayoutInline::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  // Any table-part dom child of an inline element has anonymous wrappers in the
  // layout tree so we need to climb up to the enclosing anonymous table wrapper
  // and add the new child before that.
  // TODO(rhogan): If newChild is a table part we want to insert it into the
  // same table as beforeChild.
  while (before_child && before_child->IsTablePart())
    before_child = before_child->Parent();
  if (Continuation())
    return AddChildToContinuation(new_child, before_child);
  return AddChildIgnoringContinuation(new_child, before_child);
}

static LayoutBoxModelObject* NextContinuation(LayoutObject* layout_object) {
  if (layout_object->IsInline() && !layout_object->IsAtomicInlineLevel())
    return ToLayoutInline(layout_object)->Continuation();
  return To<LayoutBlockFlow>(layout_object)->InlineElementContinuation();
}

LayoutBoxModelObject* LayoutInline::ContinuationBefore(
    LayoutObject* before_child) {
  if (before_child && before_child->Parent() == this)
    return this;

  LayoutBoxModelObject* curr = NextContinuation(this);
  LayoutBoxModelObject* next_to_last = this;
  LayoutBoxModelObject* last = this;
  while (curr) {
    if (before_child && before_child->Parent() == curr) {
      if (curr->SlowFirstChild() == before_child)
        return last;
      return curr;
    }

    next_to_last = last;
    last = curr;
    curr = NextContinuation(curr);
  }

  if (!before_child && !last->SlowFirstChild())
    return next_to_last;
  return last;
}

void LayoutInline::AddChildIgnoringContinuation(LayoutObject* new_child,
                                                LayoutObject* before_child) {
  // Make sure we don't append things after :after-generated content if we have
  // it.
  if (!before_child && IsAfterContent(LastChild()))
    before_child = LastChild();

  if (!new_child->IsInline() && !new_child->IsFloatingOrOutOfFlowPositioned() &&
      !new_child->IsTablePart()) {
    LayoutBlockFlow* new_box = CreateAnonymousContainerForBlockChildren();
    LayoutBoxModelObject* old_continuation = Continuation();
    SetContinuation(new_box);

    SplitFlow(before_child, new_box, new_child, old_continuation);
    return;
  }

  LayoutBoxModelObject::AddChild(new_child, before_child);

  new_child->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kChildChanged);
}

LayoutInline* LayoutInline::Clone() const {
  DCHECK(!IsAnonymous());
  LayoutInline* clone_inline = new LayoutInline(GetNode());
  clone_inline->SetStyle(Style());
  clone_inline->SetIsInsideFlowThread(IsInsideFlowThread());
  return clone_inline;
}

void LayoutInline::MoveChildrenToIgnoringContinuation(
    LayoutInline* to,
    LayoutObject* start_child) {
  DCHECK(!IsAnonymous());
  DCHECK(!to->IsAnonymous());
  LayoutObject* child = start_child;
  while (child) {
    LayoutObject* current_child = child;
    child = current_child->NextSibling();
    to->AddChildIgnoringContinuation(
        Children()->RemoveChildNode(this, current_child), nullptr);
  }
}

void LayoutInline::SplitInlines(LayoutBlockFlow* from_block,
                                LayoutBlockFlow* to_block,
                                LayoutBlockFlow* middle_block,
                                LayoutObject* before_child,
                                LayoutBoxModelObject* old_cont) {
  DCHECK(IsDescendantOf(from_block));
  DCHECK(!IsAnonymous());

  // FIXME: Because splitting is O(n^2) as tags nest pathologically, we cap the
  // depth at which we're willing to clone.
  // There will eventually be a better approach to this problem that will let us
  // nest to a much greater depth (see bugzilla bug 13430) but for now we have a
  // limit. This *will* result in incorrect rendering, but the alternative is to
  // hang forever.
  const unsigned kCMaxSplitDepth = 200;
  Vector<LayoutInline*> inlines_to_clone;
  LayoutInline* top_most_inline = this;
  for (LayoutObject* o = this; o != from_block; o = o->Parent()) {
    if (o->IsLayoutNGInsideListMarker())
      continue;
    top_most_inline = ToLayoutInline(o);
    if (inlines_to_clone.size() < kCMaxSplitDepth)
      inlines_to_clone.push_back(top_most_inline);
    // Keep walking up the chain to ensure |topMostInline| is a child of
    // |fromBlock|, to avoid assertion failure when |fromBlock|'s children are
    // moved to |toBlock| below.
  }

  // Create a new clone of the top-most inline in |inlinesToClone|.
  LayoutInline* top_most_inline_to_clone = inlines_to_clone.back();
  LayoutInline* clone_inline = top_most_inline_to_clone->Clone();

  // Now we are at the block level. We need to put the clone into the |toBlock|.
  to_block->Children()->AppendChildNode(to_block, clone_inline);

  // Now take all the children after |topMostInline| and remove them from the
  // |fromBlock| and put them into the toBlock.
  from_block->MoveChildrenTo(to_block, top_most_inline->NextSibling(), nullptr,
                             true);

  LayoutInline* current_parent = top_most_inline_to_clone;
  LayoutInline* clone_inline_parent = clone_inline;

  // Clone the inlines from top to down to ensure any new object will be added
  // into a rooted tree.
  // Note that we have already cloned the top-most one, so the loop begins from
  // size - 2 (except if we have reached |cMaxDepth| in which case we sacrifice
  // correct rendering for performance).
  for (int i = static_cast<int>(inlines_to_clone.size()) - 2; i >= 0; --i) {
    // Hook the clone up as a continuation of |currentInline|.
    LayoutBoxModelObject* old_cont = current_parent->Continuation();
    current_parent->SetContinuation(clone_inline);
    clone_inline->SetContinuation(old_cont);

    // Create a new clone.
    LayoutInline* current = inlines_to_clone[i];
    clone_inline = current->Clone();

    // Insert our |cloneInline| as the first child of |cloneInlineParent|.
    clone_inline_parent->AddChildIgnoringContinuation(clone_inline, nullptr);

    // Now we need to take all of the children starting from the first child
    // *after* |current| and append them all to the |cloneInlineParent|.
    current_parent->MoveChildrenToIgnoringContinuation(clone_inline_parent,
                                                       current->NextSibling());

    current_parent = current;
    clone_inline_parent = clone_inline;
  }

  // The last inline to clone is |this|, and the current |cloneInline| is cloned
  // from |this|.
  DCHECK_EQ(this, inlines_to_clone.front());

  // Hook |cloneInline| up as the continuation of the middle block.
  clone_inline->SetContinuation(old_cont);
  middle_block->SetContinuation(clone_inline);

  // Now take all of the children from |beforeChild| to the end and remove
  // them from |this| and place them in the clone.
  MoveChildrenToIgnoringContinuation(clone_inline, before_child);
}

void LayoutInline::SplitFlow(LayoutObject* before_child,
                             LayoutBlockFlow* new_block_box,
                             LayoutObject* new_child,
                             LayoutBoxModelObject* old_cont) {
  auto* block = To<LayoutBlockFlow>(ContainingBlock());
  LayoutBlockFlow* pre = nullptr;

  // Delete our line boxes before we do the inline split into continuations.
  block->DeleteLineBoxTree();

  bool reused_anonymous_block = false;
  if (block->IsAnonymousBlock()) {
    LayoutBlock* outer_containing_block = block->ContainingBlock();
    if (outer_containing_block && outer_containing_block->IsLayoutBlockFlow() &&
        !outer_containing_block->CreatesAnonymousWrapper()) {
      // We can reuse this block and make it the preBlock of the next
      // continuation.
      block->RemovePositionedObjects(nullptr);
      block->RemoveFloatingObjects();
      pre = block;
      block = To<LayoutBlockFlow>(outer_containing_block);
      reused_anonymous_block = true;
    }
  }

  // No anonymous block available for use. Make one.
  if (!reused_anonymous_block)
    pre = To<LayoutBlockFlow>(block->CreateAnonymousBlock());

  auto* post = To<LayoutBlockFlow>(pre->CreateAnonymousBlock());

  LayoutObject* box_first =
      !reused_anonymous_block ? block->FirstChild() : pre->NextSibling();
  if (!reused_anonymous_block)
    block->Children()->InsertChildNode(block, pre, box_first);
  block->Children()->InsertChildNode(block, new_block_box, box_first);
  block->Children()->InsertChildNode(block, post, box_first);
  block->SetChildrenInline(false);

  if (!reused_anonymous_block) {
    LayoutObject* o = box_first;
    while (o) {
      LayoutObject* no = o;
      o = no->NextSibling();
      pre->Children()->AppendChildNode(
          pre, block->Children()->RemoveChildNode(block, no));
      no->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
          layout_invalidation_reason::kAnonymousBlockChange);
    }
  }

  SplitInlines(pre, post, new_block_box, before_child, old_cont);

  // We already know the newBlockBox isn't going to contain inline kids, so
  // avoid wasting time in makeChildrenNonInline by just setting this explicitly
  // up front.
  new_block_box->SetChildrenInline(false);

  new_block_box->AddChild(new_child);

  // Always just do a full layout in order to ensure that line boxes (especially
  // wrappers for images) get deleted properly. Because objects moves from the
  // pre block into the post block, we want to make new line boxes instead of
  // leaving the old line boxes around.
  pre->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAnonymousBlockChange);
  block->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAnonymousBlockChange);
  post->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAnonymousBlockChange);
}

LayoutBlockFlow* LayoutInline::CreateAnonymousContainerForBlockChildren() {
  // We are placing a block inside an inline. We have to perform a split of this
  // inline into continuations. This involves creating an anonymous block box to
  // hold |newChild|. We then make that block box a continuation of this
  // inline. We take all of the children after |beforeChild| and put them in a
  // clone of this object.
  scoped_refptr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(StyleRef(),
                                                     EDisplay::kBlock);
  const LayoutBlock* containing_block = ContainingBlock();
  // The anon block we create here doesn't exist in the CSS spec, so we need to
  // ensure that any blocks it contains inherit properly from its true
  // parent. This means they must use the direction set by the anon block's
  // containing block, so we need to prevent the anon block from inheriting
  // direction from the inline. If there are any other inheritable properties
  // that apply to block and inline elements but only affect the layout of
  // children we will want to special-case them here too. Writing-mode would be
  // one if it didn't create a formatting context of its own, removing the need
  // for continuations.
  new_style->SetDirection(containing_block->StyleRef().Direction());

  // If inside an inline affected by in-flow positioning the block needs to be
  // affected by it too. Giving the block a layer like this allows it to collect
  // the x/y offsets from inline parents later.
  if (LayoutObject* positioned_ancestor = InFlowPositionedInlineAncestor(this))
    new_style->SetPosition(positioned_ancestor->StyleRef().GetPosition());

  LegacyLayout legacy = containing_block->ForceLegacyLayout()
                            ? LegacyLayout::kForce
                            : LegacyLayout::kAuto;

  return LayoutBlockFlow::CreateAnonymous(&GetDocument(), std::move(new_style),
                                          legacy);
}

void LayoutInline::AddChildToContinuation(LayoutObject* new_child,
                                          LayoutObject* before_child) {
  // A continuation always consists of two potential candidates: an inline or an
  // anonymous block box holding block children.
  LayoutBoxModelObject* flow = ContinuationBefore(before_child);
  DCHECK(!before_child || before_child->Parent()->IsAnonymousBlock() ||
         before_child->Parent()->IsLayoutInline());
  LayoutBoxModelObject* before_child_parent = nullptr;
  if (before_child) {
    before_child_parent = ToLayoutBoxModelObject(before_child->Parent());
  } else {
    LayoutBoxModelObject* cont = NextContinuation(flow);
    if (cont)
      before_child_parent = cont;
    else
      before_child_parent = flow;
  }

  // If the two candidates are the same, no further checking is necessary.
  if (flow == before_child_parent)
    return flow->AddChildIgnoringContinuation(new_child, before_child);

  // A table part will be wrapped by an inline anonymous table when it is added
  // to the layout tree, so treat it as inline when deciding where to add
  // it. Floating and out-of-flow-positioned objects can also live under
  // inlines, and since this about adding a child to an inline parent, we
  // should not put them into a block continuation.
  bool add_inside_inline = new_child->IsInline() || new_child->IsTablePart() ||
                           new_child->IsFloatingOrOutOfFlowPositioned();

  // The goal here is to match up if we can, so that we can coalesce and create
  // the minimal # of continuations needed for the inline.
  if (add_inside_inline == before_child_parent->IsInline() ||
      (before_child && before_child->IsInline())) {
    return before_child_parent->AddChildIgnoringContinuation(new_child,
                                                             before_child);
  }
  if (add_inside_inline == flow->IsInline()) {
    // Just treat like an append.
    return flow->AddChildIgnoringContinuation(new_child);
  }
  return before_child_parent->AddChildIgnoringContinuation(new_child,
                                                           before_child);
}

void LayoutInline::Paint(const PaintInfo& paint_info) const {
  InlinePainter(*this).Paint(paint_info);
}

template <typename PhysicalRectCollector>
void LayoutInline::CollectLineBoxRects(
    const PhysicalRectCollector& yield) const {
  if (IsInLayoutNGInlineFormattingContext()) {
    const auto* box_fragment = ContainingBlockFlowFragmentOf(*this);
    if (!box_fragment)
      return;
    if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
      for (const auto& fragment :
           NGInlineFragmentTraversal::SelfFragmentsOf(*box_fragment, this))
        yield(fragment.RectInContainerBox());
      return;
    }
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      yield(cursor.Current().RectInContainerBlock());
    return;
  }
  if (!AlwaysCreateLineBoxes()) {
    CollectCulledLineBoxRects(yield);
  } else {
    const LayoutBlock* block_for_flipping =
        UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
    for (InlineFlowBox* curr : *LineBoxes()) {
      yield(FlipForWritingMode(LayoutRect(curr->Location(), curr->Size()),
                               block_for_flipping));
    }
  }
}

template <typename PhysicalRectCollector>
void LayoutInline::CollectCulledLineBoxRects(
    const PhysicalRectCollector& yield) const {
  DCHECK(!IsInLayoutNGInlineFormattingContext());
  const LayoutBlock* block_for_flipping =
      UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
  CollectCulledLineBoxRectsInFlippedBlocksDirection(
      [this, block_for_flipping, &yield](const LayoutRect& r) {
        PhysicalRect rect = FlipForWritingMode(r, block_for_flipping);
        yield(rect);
      },
      this);
}

static inline void ComputeItemTopHeight(const LayoutInline* container,
                                        const RootInlineBox& root_box,
                                        LayoutUnit* top,
                                        LayoutUnit* height) {
  bool first_line = root_box.IsFirstLineStyle();
  const SimpleFontData* font_data =
      root_box.GetLineLayoutItem().Style(first_line)->GetFont().PrimaryFont();
  const SimpleFontData* container_font_data =
      container->Style(first_line)->GetFont().PrimaryFont();
  DCHECK(font_data);
  DCHECK(container_font_data);
  if (!font_data || !container_font_data) {
    *top = LayoutUnit();
    *height = LayoutUnit();
    return;
  }
  auto metrics = font_data->GetFontMetrics();
  auto container_metrics = container_font_data->GetFontMetrics();
  *top =
      root_box.LogicalTop() + (metrics.Ascent() - container_metrics.Ascent());
  *height = LayoutUnit(container_metrics.Height());
}

template <typename FlippedRectCollector>
void LayoutInline::CollectCulledLineBoxRectsInFlippedBlocksDirection(
    const FlippedRectCollector& yield,
    const LayoutInline* container) const {
  if (!CulledInlineFirstLineBox())
    return;

  bool is_horizontal = StyleRef().IsHorizontalWritingMode();

  LayoutUnit logical_top, logical_height;
  for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // We want to get the margin box in the inline direction, and then use our
    // font ascent/descent in the block direction (aligned to the root box's
    // baseline).
    if (curr->IsBox()) {
      LayoutBox* curr_box = ToLayoutBox(curr);
      if (curr_box->InlineBoxWrapper()) {
        RootInlineBox& root_box = curr_box->InlineBoxWrapper()->Root();
        ComputeItemTopHeight(container, root_box, &logical_top,
                             &logical_height);
        if (is_horizontal) {
          yield(LayoutRect(
              curr_box->InlineBoxWrapper()->X() - curr_box->MarginLeft(),
              logical_top, curr_box->Size().Width() + curr_box->MarginWidth(),
              logical_height));
        } else {
          yield(LayoutRect(
              logical_top,
              curr_box->InlineBoxWrapper()->Y() - curr_box->MarginTop(),
              logical_height,
              curr_box->Size().Height() + curr_box->MarginHeight()));
        }
      }
    } else if (curr->IsLayoutInline()) {
      // If the child doesn't need line boxes either, then we can recur.
      LayoutInline* curr_inline = ToLayoutInline(curr);
      if (!curr_inline->AlwaysCreateLineBoxes()) {
        curr_inline->CollectCulledLineBoxRectsInFlippedBlocksDirection(
            yield, container);
      } else {
        for (InlineFlowBox* child_line : *curr_inline->LineBoxes()) {
          RootInlineBox& root_box = child_line->Root();
          ComputeItemTopHeight(container, root_box, &logical_top,
                               &logical_height);
          LayoutUnit logical_width =
              child_line->LogicalWidth() + child_line->MarginLogicalWidth();
          if (is_horizontal) {
            yield(LayoutRect(
                LayoutUnit(child_line->X() - child_line->MarginLogicalLeft()),
                logical_top, logical_width, logical_height));
          } else {
            yield(LayoutRect(
                logical_top,
                LayoutUnit(child_line->Y() - child_line->MarginLogicalLeft()),
                logical_height, logical_width));
          }
        }
      }
    } else if (curr->IsText()) {
      LayoutText* curr_text = ToLayoutText(curr);
      for (InlineTextBox* child_text : curr_text->TextBoxes()) {
        RootInlineBox& root_box = child_text->Root();
        ComputeItemTopHeight(container, root_box, &logical_top,
                             &logical_height);
        if (is_horizontal)
          yield(LayoutRect(child_text->X(), logical_top,
                           child_text->LogicalWidth(), logical_height));
        else
          yield(LayoutRect(logical_top, child_text->Y(), logical_height,
                           child_text->LogicalWidth()));
      }
    }
  }
}

void LayoutInline::AbsoluteQuadsForSelf(Vector<FloatQuad>& quads,
                                        MapCoordinatesFlags mode) const {
  LayoutGeometryMap geometry_map(mode);
  geometry_map.PushMappingsToAncestor(this, nullptr);
  CollectLineBoxRects([&quads, &geometry_map](const PhysicalRect& r) {
    quads.push_back(geometry_map.AbsoluteQuad(r));
  });
  if (quads.IsEmpty())
    quads.push_back(geometry_map.AbsoluteQuad(PhysicalRect()));
}

base::Optional<PhysicalOffset> LayoutInline::FirstLineBoxTopLeftInternal()
    const {
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    if (!cursor)
      return base::nullopt;
    return cursor.Current().OffsetInContainerBlock();
  }
  if (const InlineBox* first_box = FirstLineBoxIncludingCulling()) {
    LayoutPoint location = first_box->Location();
    if (UNLIKELY(HasFlippedBlocksWritingMode())) {
      location.Move(first_box->Width(), LayoutUnit());
      return ContainingBlock()->FlipForWritingMode(location);
    }
    return PhysicalOffset(location);
  }
  return base::nullopt;
}

PhysicalOffset LayoutInline::AnchorPhysicalLocation() const {
  if (const auto& location = FirstLineBoxTopLeftInternal())
    return *location;
  // This object doesn't have fragment/line box, probably because it's an empty
  // and at the beginning/end of a line. Query sibling or parent.
  // TODO(crbug.com/953479): We won't need this if we always create line box
  // for empty inline elements. The following algorithm works in most cases for
  // anchor elements, though may be inaccurate in some corner cases (e.g. if the
  // sibling is not in the same line).
  if (const auto* sibling = NextSibling()) {
    if (sibling->IsLayoutInline())
      return ToLayoutInline(sibling)->AnchorPhysicalLocation();
    if (sibling->IsText())
      return ToLayoutText(sibling)->FirstLineBoxTopLeft();
    if (sibling->IsBox())
      return ToLayoutBox(sibling)->PhysicalLocation();
  }
  if (Parent()->IsLayoutInline())
    return ToLayoutInline(Parent())->AnchorPhysicalLocation();
  return PhysicalOffset();
}

PhysicalRect LayoutInline::AbsoluteBoundingBoxRectHandlingEmptyInline() const {
  Vector<PhysicalRect> rects = OutlineRects(
      PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  PhysicalRect rect = UnionRect(rects);
  // When empty LayoutInline is not culled, |rect| is empty but |rects| is not.
  if (rect.IsEmpty())
    rect.offset = AnchorPhysicalLocation();
  return LocalToAbsoluteRect(rect);
}

LayoutUnit LayoutInline::OffsetLeft(const Element* parent) const {
  return AdjustedPositionRelativeTo(FirstLineBoxTopLeft(), parent).left;
}

LayoutUnit LayoutInline::OffsetTop(const Element* parent) const {
  return AdjustedPositionRelativeTo(FirstLineBoxTopLeft(), parent).top;
}

static LayoutUnit ComputeMargin(const LayoutInline* layout_object,
                                const Length& margin) {
  if (margin.IsFixed())
    return LayoutUnit(margin.Value());
  if (margin.IsPercentOrCalc())
    return MinimumValueForLength(
        margin,
        std::max(LayoutUnit(),
                 layout_object->ContainingBlock()->AvailableLogicalWidth()));
  return LayoutUnit();
}

LayoutUnit LayoutInline::MarginLeft() const {
  return ComputeMargin(this, StyleRef().MarginLeft());
}

LayoutUnit LayoutInline::MarginRight() const {
  return ComputeMargin(this, StyleRef().MarginRight());
}

LayoutUnit LayoutInline::MarginTop() const {
  return ComputeMargin(this, StyleRef().MarginTop());
}

LayoutUnit LayoutInline::MarginBottom() const {
  return ComputeMargin(this, StyleRef().MarginBottom());
}

bool LayoutInline::NodeAtPoint(HitTestResult& result,
                               const HitTestLocation& hit_test_location,
                               const PhysicalOffset& accumulated_offset,
                               HitTestAction hit_test_action) {
  if (ContainingNGBlockFlow()) {
    // TODO(crbug.com/965976): We should fix the root cause of the missed
    // layout.
    if (UNLIKELY(NeedsLayout())) {
      NOTREACHED();
      return false;
    }

    // In LayoutNG, we reach here only when called from
    // PaintLayer::HitTestContents() without going through any ancestor, in
    // which case the element must have self painting layer.
    DCHECK(HasSelfPaintingLayer());
    NGInlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      if (const NGPaintFragment* paint_fragment =
              cursor.Current().PaintFragment()) {
        // NGBoxFragmentPainter::NodeAtPoint() takes an offset that is
        // accumulated up to the fragment itself. Compute this offset.
        const PhysicalOffset child_offset =
            accumulated_offset + paint_fragment->OffsetInContainerBlock();
        if (NGBoxFragmentPainter(*paint_fragment)
                .NodeAtPoint(result, hit_test_location, child_offset,
                             hit_test_action))
          return true;
        continue;
      }
      DCHECK(cursor.Current().Item());
      const NGFragmentItem& item = *cursor.Current().Item();
      const NGPhysicalBoxFragment* box_fragment = item.BoxFragment();
      DCHECK(box_fragment);
      // NGBoxFragmentPainter::NodeAtPoint() takes an offset that is accumulated
      // up to the fragment itself. Compute this offset.
      const PhysicalOffset child_offset =
          accumulated_offset + item.OffsetInContainerBlock();
      if (NGBoxFragmentPainter(cursor, item, *box_fragment)
              .NodeAtPoint(result, hit_test_location, child_offset,
                           accumulated_offset, hit_test_action))
        return true;
    }
    return false;
  }

  return LineBoxes()->HitTest(LineLayoutBoxModel(this), result,
                              hit_test_location, accumulated_offset,
                              hit_test_action);
}

bool LayoutInline::HitTestCulledInline(HitTestResult& result,
                                       const HitTestLocation& hit_test_location,
                                       const PhysicalOffset& accumulated_offset,
                                       const NGInlineCursor* parent_cursor) {
  DCHECK(parent_cursor || !AlwaysCreateLineBoxes());
  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  HitTestLocation adjusted_location(hit_test_location, -accumulated_offset);
  Region region_result;
  bool intersected = false;
  auto yield = [&adjusted_location, &region_result,
                &intersected](const PhysicalRect& rect) {
    if (adjusted_location.Intersects(rect)) {
      intersected = true;
      region_result.Unite(EnclosingIntRect(rect));
    }
  };

  // NG generates purely physical rectangles here, while legacy sets the block
  // offset on the rectangles relatively to the block-start. NG is doing the
  // right thing. Legacy is wrong.
  if (parent_cursor) {
    // Iterate fragments for |this|, including culled inline, but only that are
    // descendants of |parent_cursor|.
    DCHECK(IsDescendantOf(parent_cursor->GetLayoutBlockFlow()));
    NGInlineCursor cursor(*parent_cursor);
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      yield(cursor.Current().RectInContainerBlock());
  } else {
    DCHECK(!ContainingNGBlockFlow());
    CollectCulledLineBoxRects(yield);
  }

  if (intersected) {
    UpdateHitTestResult(result, adjusted_location.Point());
    if (result.AddNodeToListBasedTestResult(GetNode(), adjusted_location,
                                            region_result) == kStopHitTesting)
      return true;
  }
  return false;
}

PositionWithAffinity LayoutInline::PositionForPoint(
    const PhysicalOffset& point) const {
  // FIXME: Does not deal with relative positioned inlines (should it?)

  // If there are continuations, test them first because our containing block
  // will not check them.
  // TODO(layout-dev): Handle continuation with NG structures when NG has its
  // own tree building.
  LayoutBoxModelObject* continuation = Continuation();
  while (continuation) {
    if (continuation->IsInline() || continuation->SlowFirstChild())
      return continuation->PositionForPoint(point);
    continuation =
        To<LayoutBlockFlow>(continuation)->InlineElementContinuation();
  }

  if (const LayoutBlockFlow* ng_block_flow = ContainingNGBlockFlow())
    return ng_block_flow->PositionForPoint(*this, point);

  DCHECK(CanUseInlineBox(*this));

  if (FirstLineBoxIncludingCulling()) {
    // This inline actually has a line box.  We must have clicked in the
    // border/padding of one of these boxes.  We
    // should try to find a result by asking our containing block.
    return ContainingBlock()->PositionForPoint(point);
  }

  return LayoutBoxModelObject::PositionForPoint(point);
}

PhysicalRect LayoutInline::PhysicalLinesBoundingBox() const {
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    PhysicalRect bounding_box;
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      bounding_box.UniteIfNonZero(cursor.Current().RectInContainerBlock());
    return bounding_box;
  }

  if (!AlwaysCreateLineBoxes()) {
    DCHECK(!FirstLineBox());
    PhysicalRect bounding_box;
    CollectLineBoxRects([&bounding_box](const PhysicalRect& rect) {
      bounding_box.UniteIfNonZero(rect);
    });
    return bounding_box;
  }

  LayoutRect result;

  // See <rdar://problem/5289721>, for an unknown reason the linked list here is
  // sometimes inconsistent, first is non-zero and last is zero.  We have been
  // unable to reproduce this at all (and consequently unable to figure ot why
  // this is happening).  The assert will hopefully catch the problem in debug
  // builds and help us someday figure out why.  We also put in a redundant
  // check of lastLineBox() to avoid the crash for now.
  DCHECK_EQ(!FirstLineBox(),
            !LastLineBox());  // Either both are null or both exist.
  if (FirstLineBox() && LastLineBox()) {
    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logical_left_side;
    LayoutUnit logical_right_side;
    for (InlineFlowBox* curr : *LineBoxes()) {
      if (curr == FirstLineBox() || curr->LogicalLeft() < logical_left_side)
        logical_left_side = curr->LogicalLeft();
      if (curr == FirstLineBox() || curr->LogicalRight() > logical_right_side)
        logical_right_side = curr->LogicalRight();
    }

    bool is_horizontal = StyleRef().IsHorizontalWritingMode();

    LayoutUnit x = is_horizontal ? logical_left_side : FirstLineBox()->X();
    LayoutUnit y = is_horizontal ? FirstLineBox()->Y() : logical_left_side;
    LayoutUnit width = is_horizontal ? logical_right_side - logical_left_side
                                     : LastLineBox()->LogicalBottom() - x;
    LayoutUnit height = is_horizontal ? LastLineBox()->LogicalBottom() - y
                                      : logical_right_side - logical_left_side;
    result = LayoutRect(x, y, width, height);
  }

  return FlipForWritingMode(result);
}

InlineBox* LayoutInline::CulledInlineFirstLineBox() const {
  for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // We want to get the margin box in the inline direction, and then use our
    // font ascent/descent in the block direction (aligned to the root box's
    // baseline).
    if (curr->IsBox())
      return ToLayoutBox(curr)->InlineBoxWrapper();
    if (curr->IsLayoutInline()) {
      LayoutInline* curr_inline = ToLayoutInline(curr);
      InlineBox* result = curr_inline->FirstLineBoxIncludingCulling();
      if (result)
        return result;
    } else if (curr->IsText()) {
      LayoutText* curr_text = ToLayoutText(curr);
      if (curr_text->FirstTextBox())
        return curr_text->FirstTextBox();
    }
  }
  return nullptr;
}

InlineBox* LayoutInline::CulledInlineLastLineBox() const {
  for (LayoutObject* curr = LastChild(); curr; curr = curr->PreviousSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // We want to get the margin box in the inline direction, and then use our
    // font ascent/descent in the block direction (aligned to the root box's
    // baseline).
    if (curr->IsBox())
      return ToLayoutBox(curr)->InlineBoxWrapper();
    if (curr->IsLayoutInline()) {
      LayoutInline* curr_inline = ToLayoutInline(curr);
      InlineBox* result = curr_inline->LastLineBoxIncludingCulling();
      if (result)
        return result;
    } else if (curr->IsText()) {
      LayoutText* curr_text = ToLayoutText(curr);
      if (curr_text->LastTextBox())
        return curr_text->LastTextBox();
    }
  }
  return nullptr;
}

PhysicalRect LayoutInline::CulledInlineVisualOverflowBoundingBox() const {
  PhysicalRect result;
  CollectCulledLineBoxRects(
      [&result](const PhysicalRect& r) { result.UniteIfNonZero(r); });
  if (!FirstChild())
    return result;

  bool is_horizontal = StyleRef().IsHorizontalWritingMode();
  const LayoutBlock* block_for_flipping =
      UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
  for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // For overflow we just have to propagate by hand and recompute it all.
    if (curr->IsBox()) {
      LayoutBox* curr_box = ToLayoutBox(curr);
      if (!curr_box->HasSelfPaintingLayer() && curr_box->InlineBoxWrapper()) {
        LayoutRect logical_rect =
            curr_box->LogicalVisualOverflowRectForPropagation();
        if (is_horizontal) {
          logical_rect.MoveBy(curr_box->Location());
          result.UniteIfNonZero(PhysicalRect(logical_rect));
        } else {
          logical_rect.MoveBy(curr_box->Location());
          result.UniteIfNonZero(FlipForWritingMode(
              logical_rect.TransposedRect(), block_for_flipping));
        }
      }
    } else if (curr->IsLayoutInline()) {
      // If the child doesn't need line boxes either, then we can recur.
      LayoutInline* curr_inline = ToLayoutInline(curr);
      if (!curr_inline->AlwaysCreateLineBoxes()) {
        result.UniteIfNonZero(
            curr_inline->CulledInlineVisualOverflowBoundingBox());
      } else if (!curr_inline->HasSelfPaintingLayer()) {
        result.UniteIfNonZero(curr_inline->PhysicalVisualOverflowRect());
      }
    } else if (curr->IsText()) {
      LayoutText* curr_text = ToLayoutText(curr);
      result.UniteIfNonZero(curr_text->PhysicalVisualOverflowRect());
    }
  }
  return result;
}

PhysicalRect LayoutInline::LinesVisualOverflowBoundingBox() const {
  if (IsInLayoutNGInlineFormattingContext()) {
    PhysicalRect result;
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      PhysicalRect child_rect = cursor.Current().InkOverflow();
      child_rect.offset += cursor.Current().OffsetInContainerBlock();
      result.Unite(child_rect);
    }
    return result;
  }

  if (!AlwaysCreateLineBoxes())
    return CulledInlineVisualOverflowBoundingBox();

  if (!FirstLineBox() || !LastLineBox())
    return PhysicalRect();

  // Return the width of the minimal left side and the maximal right side.
  LayoutUnit logical_left_side = LayoutUnit::Max();
  LayoutUnit logical_right_side = LayoutUnit::Min();
  for (InlineFlowBox* curr : *LineBoxes()) {
    logical_left_side =
        std::min(logical_left_side, curr->LogicalLeftVisualOverflow());
    logical_right_side =
        std::max(logical_right_side, curr->LogicalRightVisualOverflow());
  }

  RootInlineBox& first_root_box = FirstLineBox()->Root();
  RootInlineBox& last_root_box = LastLineBox()->Root();

  LayoutUnit logical_top =
      FirstLineBox()->LogicalTopVisualOverflow(first_root_box.LineTop());
  LayoutUnit logical_width = logical_right_side - logical_left_side;
  LayoutUnit logical_height =
      LastLineBox()->LogicalBottomVisualOverflow(last_root_box.LineBottom()) -
      logical_top;

  LayoutRect rect(logical_left_side, logical_top, logical_width,
                  logical_height);
  if (!StyleRef().IsHorizontalWritingMode())
    rect = rect.TransposedRect();
  return FlipForWritingMode(rect);
}

PhysicalRect LayoutInline::VisualRectInDocument(VisualRectFlags flags) const {
  PhysicalRect rect;
  if (!Continuation()) {
    rect = PhysicalVisualOverflowRect();
  } else {
    // Should also cover continuations.
    rect = UnionRect(OutlineRects(PhysicalOffset(),
                                  NGOutlineType::kIncludeBlockVisualOverflow));
  }
  MapToVisualRectInAncestorSpace(View(), rect, flags);
  return rect;
}

PhysicalRect LayoutInline::LocalVisualRectIgnoringVisibility() const {
  if (IsInLayoutNGInlineFormattingContext()) {
    DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
    if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
      return NGFragmentItem::LocalVisualRectFor(*this);

    if (const auto& visual_rect = NGPaintFragment::LocalVisualRectFor(*this))
      return *visual_rect;
  }

  // If we don't create line boxes, we don't have any invalidations to do.
  if (!AlwaysCreateLineBoxes())
    return PhysicalRect();

  // VisualOverflowRect() is in "physical coordinates with flipped blocks
  // direction", while all "VisualRect"s are in pure physical coordinates.
  return PhysicalVisualOverflowRect();
}

PhysicalRect LayoutInline::PhysicalVisualOverflowRect() const {
  PhysicalRect overflow_rect = LinesVisualOverflowBoundingBox();
  LayoutUnit outline_outset(StyleRef().OutlineOutsetExtent());
  if (outline_outset) {
    Vector<PhysicalRect> rects;
    if (GetDocument().InNoQuirksMode()) {
      // We have already included outline extents of line boxes in
      // linesVisualOverflowBoundingBox(), so the following just add outline
      // rects for children and continuations.
      AddOutlineRectsForChildrenAndContinuations(
          rects, PhysicalOffset(),
          OutlineRectsShouldIncludeBlockVisualOverflow());
    } else {
      // In non-standard mode, because the difference in
      // LayoutBlock::minLineHeightForReplacedObject(),
      // linesVisualOverflowBoundingBox() may not cover outline rects of lines
      // containing replaced objects.
      AddOutlineRects(rects, PhysicalOffset(),
                      OutlineRectsShouldIncludeBlockVisualOverflow());
    }
    if (!rects.IsEmpty()) {
      PhysicalRect outline_rect = UnionRectEvenIfEmpty(rects);
      outline_rect.Inflate(outline_outset);
      overflow_rect.Unite(outline_rect);
    }
  }
  return overflow_rect;
}

PhysicalRect LayoutInline::ReferenceBoxForClipPath() const {
  // The spec just says to use the border box as clip-path reference box. It
  // doesn't say what to do if there are multiple lines. Gecko uses the first
  // fragment in that case. We'll do the same here (but correctly with respect
  // to writing-mode - Gecko has some issues there).
  // See crbug.com/641907
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor)
      return cursor.Current().RectInContainerBlock();
  }
  if (const InlineFlowBox* flow_box = FirstLineBox())
    return FlipForWritingMode(flow_box->FrameRect());
  return PhysicalRect();
}

bool LayoutInline::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  if (ancestor == this)
    return true;

  LayoutObject* container = Container();
  DCHECK_EQ(container, Parent());
  if (!container)
    return true;

  bool preserve3d = container->StyleRef().Preserves3D();

  TransformState::TransformAccumulation accumulation =
      preserve3d ? TransformState::kAccumulateTransform
                 : TransformState::kFlattenTransform;

  if (StyleRef().HasInFlowPosition() && Layer()) {
    // Apply the in-flow position offset when invalidating a rectangle. The
    // layer is translated, but the layout box isn't, so we need to do this to
    // get the right dirty rect. Since this is called from LayoutObject::
    // setStyle, the relative position flag on the LayoutObject has been
    // cleared, so use the one on the style().
    transform_state.Move(Layer()->GetLayoutObject().OffsetForInFlowPosition(),
                         accumulation);
  }

  LayoutBox* container_box =
      container->IsBox() ? ToLayoutBox(container) : nullptr;
  if (container_box && container != ancestor &&
      !container_box->MapContentsRectToBoxSpace(transform_state, accumulation,
                                                *this, visual_rect_flags))
    return false;

  return container->MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

PhysicalOffset LayoutInline::OffsetFromContainerInternal(
    const LayoutObject* container,
    bool ignore_scroll_offset) const {
  DCHECK_EQ(container, Container());

  PhysicalOffset offset;
  if (IsInFlowPositioned())
    offset += OffsetForInFlowPosition();

  if (container->HasOverflowClip())
    offset += OffsetFromScrollableContainer(container, ignore_scroll_offset);

  return offset;
}

PaintLayerType LayoutInline::LayerTypeRequired() const {
  return IsInFlowPositioned() || CreatesGroup() ||
                 StyleRef().ShouldCompositeForCurrentAnimations() ||
                 ShouldApplyPaintContainment()
             ? kNormalPaintLayer
             : kNoPaintLayer;
}

void LayoutInline::ChildBecameNonInline(LayoutObject* child) {
  // We have to split the parent flow.
  LayoutBlockFlow* new_box = CreateAnonymousContainerForBlockChildren();
  LayoutBoxModelObject* old_continuation = Continuation();
  SetContinuation(new_box);
  LayoutObject* before_child = child->NextSibling();
  Children()->RemoveChildNode(this, child);
  SplitFlow(before_child, new_box, child, old_continuation);
}

void LayoutInline::UpdateHitTestResult(HitTestResult& result,
                                       const PhysicalOffset& point) const {
  if (result.InnerNode())
    return;

  Node* n = GetNode();
  PhysicalOffset local_point = point;
  if (n) {
    if (IsInlineElementContinuation()) {
      // We're in the continuation of a split inline. Adjust our local point to
      // be in the coordinate space of the principal layoutObject's containing
      // block. This will end up being the innerNode.
      LayoutBlock* first_block = n->GetLayoutObject()->ContainingBlock();

      // Get our containing block.
      LayoutBox* block = ContainingBlock();
      local_point += block->PhysicalLocation();
      local_point -= first_block->PhysicalLocation();
    }

    result.SetNodeAndPosition(n, local_point);
  }
}

void LayoutInline::DirtyLineBoxes(bool full_layout) {
  if (full_layout) {
    DeleteLineBoxes();
    return;
  }

  if (!AlwaysCreateLineBoxes()) {
    // We have to grovel into our children in order to dirty the appropriate
    // lines.
    for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
      if (curr->IsFloatingOrOutOfFlowPositioned())
        continue;
      if (curr->IsBox() && !curr->NeedsLayout()) {
        LayoutBox* curr_box = ToLayoutBox(curr);
        if (curr_box->InlineBoxWrapper())
          curr_box->InlineBoxWrapper()->Root().MarkDirty();
      } else if (!curr->SelfNeedsLayout()) {
        if (curr->IsLayoutInline()) {
          LayoutInline* curr_inline = ToLayoutInline(curr);
          for (InlineFlowBox* child_line : *curr_inline->LineBoxes())
            child_line->Root().MarkDirty();
        } else if (curr->IsText()) {
          LayoutText* curr_text = ToLayoutText(curr);
          for (InlineTextBox* child_text : curr_text->TextBoxes())
            child_text->Root().MarkDirty();
        }
      }
    }
  } else {
    MutableLineBoxes()->DirtyLineBoxes();
  }
}

InlineFlowBox* LayoutInline::CreateInlineFlowBox() {
  return new InlineFlowBox(LineLayoutItem(this));
}

InlineFlowBox* LayoutInline::CreateAndAppendInlineFlowBox() {
  SetAlwaysCreateLineBoxes();
  InlineFlowBox* flow_box = CreateInlineFlowBox();
  MutableLineBoxes()->AppendLineBox(flow_box);
  return flow_box;
}

void LayoutInline::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  if (IsInLayoutNGInlineFormattingContext()) {
    if (UNLIKELY(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())) {
      if (const LayoutBlockFlow* container = FragmentItemsContainer()) {
        if (const NGFragmentItems* items = container->FragmentItems())
          items->DirtyLinesFromChangedChild(child);
      }
      return;
    }
    SetAncestorLineBoxDirty();
    return;
  }
  MutableLineBoxes()->DirtyLinesFromChangedChild(
      LineLayoutItem(this), LineLayoutItem(child),
      marking_behavior == kMarkContainerChain);
}

LayoutUnit LayoutInline::LineHeight(
    bool first_line,
    LineDirectionMode /*direction*/,
    LinePositionMode /*linePositionMode*/) const {
  if (first_line && GetDocument().GetStyleEngine().UsesFirstLineRules()) {
    const ComputedStyle* s = Style(first_line);
    if (s != Style())
      return LayoutUnit(s->ComputedLineHeight());
  }

  return LayoutUnit(StyleRef().ComputedLineHeight());
}

LayoutUnit LayoutInline::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  const SimpleFontData* font_data = Style(first_line)->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit(-1);
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  return LayoutUnit((font_metrics.Ascent(baseline_type) +
                     (LineHeight(first_line, direction, line_position_mode) -
                      font_metrics.Height()) /
                         2)
                        .ToInt());
}

PhysicalOffset LayoutInline::OffsetForInFlowPositionedInline(
    const LayoutBox& child) const {
  // TODO(layout-dev): This function isn't right with mixed writing modes,
  // but LayoutNG has fixed the issue. This function seems to always return
  // zero in LayoutNG. We should probably remove this function for LayoutNG.

  DCHECK(IsInFlowPositioned() || StyleRef().HasNonInitialFilter() ||
         StyleRef().HasNonInitialBackdropFilter());
  if (!IsInFlowPositioned() && !StyleRef().HasNonInitialFilter() &&
      !StyleRef().HasNonInitialBackdropFilter()) {
    DCHECK(CreatesGroup())
        << "Inlines with filters or backdrop-filters should create a group";
    return PhysicalOffset();
  }

  // When we have an enclosing relpositioned inline, we need to add in the
  // offset of the first line box from the rest of the content, but only in the
  // cases where we know we're positioned relative to the inline itself.

  LayoutSize logical_offset;
  LayoutUnit inline_position;
  LayoutUnit block_position;
  if (FirstLineBox()) {
    inline_position = FirstLineBox()->LogicalLeft();
    block_position = FirstLineBox()->LogicalTop();
  } else {
    DCHECK(Layer());
    inline_position = Layer()->StaticInlinePosition();
    block_position = Layer()->StaticBlockPosition();
  }

  // Per http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width an
  // absolute positioned box with a static position should locate itself as
  // though it is a normal flow box in relation to its containing block.
  if (!child.StyleRef().HasStaticInlinePosition(
          StyleRef().IsHorizontalWritingMode()))
    logical_offset.SetWidth(inline_position);

  if (!child.StyleRef().HasStaticBlockPosition(
          StyleRef().IsHorizontalWritingMode()))
    logical_offset.SetHeight(block_position);

  return PhysicalOffset(StyleRef().IsHorizontalWritingMode()
                            ? logical_offset
                            : logical_offset.TransposedSize());
}

void LayoutInline::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  if (!Parent())
    return;

  // FIXME: We can do better.
  SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kImage);
}

void LayoutInline::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
#if DCHECK_IS_ON()
  // TODO(crbug.com/987836): enable this DCHECK universally.
  Page* page = GetDocument().GetPage();
  if (page && !page->GetSettings().GetSpatialNavigationEnabled()) {
    DCHECK_GE(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kAfterPerformLayout);
  }
#endif  // DCHECK_IS_ON()

  CollectLineBoxRects([&rects, &additional_offset](const PhysicalRect& r) {
    auto rect = r;
    rect.Move(additional_offset);
    rects.push_back(rect);
  });
  AddOutlineRectsForChildrenAndContinuations(rects, additional_offset,
                                             include_block_overflows);
}

void LayoutInline::AddOutlineRectsForChildrenAndContinuations(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  AddOutlineRectsForNormalChildren(rects, additional_offset,
                                   include_block_overflows);
  AddOutlineRectsForContinuations(rects, additional_offset,
                                  include_block_overflows);
}

void LayoutInline::AddOutlineRectsForContinuations(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  if (LayoutBoxModelObject* continuation = Continuation()) {
    if (continuation->NeedsLayout()) {
      // TODO(mstensho): Prevent this from happening. Before we can get the
      // outlines of an object, we obviously need to have laid it out. We may
      // currently end up in a situation where the continuation's layout isn't
      // up-to-date here. This happens when calculating the overflow rectangle
      // while laying out one of the earlier anonymous blocks that form the
      // continuation chain. The outlines from the continuations should probably
      // not be part of the overflow rectangle of that anonymous block at
      // all. Yet, here we are. Bail.
      return;
    }
    PhysicalOffset offset = additional_offset;
    if (continuation->IsInline())
      offset += continuation->ContainingBlock()->PhysicalLocation();
    else
      offset += ToLayoutBox(continuation)->PhysicalLocation();
    offset -= ContainingBlock()->PhysicalLocation();
    continuation->AddOutlineRects(rects, offset, include_block_overflows);
  }
}

FloatRect LayoutInline::LocalBoundingBoxRectForAccessibility() const {
  Vector<PhysicalRect> rects = OutlineRects(
      PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  return FloatRect(FlipForWritingMode(UnionRect(rects).ToLayoutRect()));
}

void LayoutInline::AddAnnotatedRegions(Vector<AnnotatedRegionValue>& regions) {
  // Convert the style regions to absolute coordinates.
  if (StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (StyleRef().DraggableRegionMode() == EDraggableRegionMode::kNone)
    return;

  AnnotatedRegionValue region;
  region.draggable =
      StyleRef().DraggableRegionMode() == EDraggableRegionMode::kDrag;
  region.bounds = PhysicalLinesBoundingBox();
  // TODO(crbug.com/966048): We probably want to also cover continuations.

  LayoutObject* container = ContainingBlock();
  if (!container)
    container = this;

  // TODO(crbug.com/966048): The kIgnoreTransforms seems incorrect. We probably
  // want to map visual rect (with clips applied).
  region.bounds.offset +=
      container->LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms);
  regions.push_back(region);
}

void LayoutInline::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  ObjectPaintInvalidator paint_invalidator(*this);

  if (RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled() &&
      !RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    auto fragments = NGPaintFragment::InlineFragmentsFor(this);
    if (fragments.IsInLayoutNGInlineFormattingContext()) {
      for (NGPaintFragment* fragment : fragments) {
        paint_invalidator.InvalidateDisplayItemClient(*fragment,
                                                      invalidation_reason);
      }
      return;
    }
  }

  if (IsInLayoutNGInlineFormattingContext()) {
    if (!ShouldCreateBoxFragment())
      return;
    NGInlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      DCHECK_EQ(cursor.Current().GetLayoutObject(), this);
      paint_invalidator.InvalidateDisplayItemClient(
          *cursor.Current().GetDisplayItemClient(), invalidation_reason);
    }
    return;
  }

  paint_invalidator.InvalidateDisplayItemClient(*this, invalidation_reason);

  for (InlineFlowBox* box : *LineBoxes())
    paint_invalidator.InvalidateDisplayItemClient(*box, invalidation_reason);
}

void LayoutInline::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                      TransformState& transform_state,
                                      MapCoordinatesFlags mode) const {
  if (CanContainFixedPositionObjects())
    mode &= ~kIsFixed;
  LayoutBoxModelObject::MapLocalToAncestor(ancestor, transform_state, mode);
}

PhysicalRect LayoutInline::DebugRect() const {
  return PhysicalRect(EnclosingIntRect(PhysicalLinesBoundingBox()));
}

}  // namespace blink

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

#include "core/layout/LayoutInline.h"

#include "core/css/StyleEngine.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/InlinePainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/Region.h"
#include "platform/geometry/TransformState.h"

namespace blink {

namespace {

// TODO(layout-dev): Once we generate fragment for all inline element, we should
// use |LayoutObject::EnclosingBlockFlowFragment()|.
const NGPhysicalBoxFragment* EnclosingBlockFlowFragmentOf(
    const LayoutInline& node) {
  if (!RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled())
    return nullptr;
  return node.EnclosingBlockFlowFragment();
}

}  // anonymous namespace

struct SameSizeAsLayoutInline : public LayoutBoxModelObject {
  ~SameSizeAsLayoutInline() override = default;
  LayoutObjectChildList children_;
  LineBoxList line_boxes_;
};

static_assert(sizeof(LayoutInline) == sizeof(SameSizeAsLayoutInline),
              "LayoutInline should stay small");

LayoutInline::LayoutInline(Element* element) : LayoutBoxModelObject(element) {
  SetChildrenInline(true);
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
      // We can't wait for LayoutBoxModelObject::destroy to clear the selection,
      // because by then we will have nuked the line boxes.
      // FIXME: The FrameSelection should be responsible for this when it
      // is notified of DOM mutations.
      if (IsSelectionBorder())
        View()->ClearSelection();

      // If line boxes are contained inside a root, that means we're an inline.
      // In that case, we need to remove all the line boxes so that the parent
      // lines aren't pointing to deleted children. If the first line box does
      // not have a parent that means they are either already disconnected or
      // root lines that can just be destroyed without disconnecting.
      if (FirstLineBox()->Parent()) {
        for (InlineFlowBox* box = FirstLineBox(); box; box = box->NextLineBox())
          box->Remove();
      }
    } else if (Parent()) {
      Parent()->DirtyLinesFromChangedChild(this);
    }
  }

  line_boxes_.DeleteLineBoxes();

  LayoutBoxModelObject::WillBeDestroyed();
}

LayoutInline* LayoutInline::InlineElementContinuation() const {
  LayoutBoxModelObject* continuation = Continuation();
  if (!continuation || continuation->IsInline())
    return ToLayoutInline(continuation);
  return ToLayoutBlockFlow(continuation)->InlineElementContinuation();
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
    LayoutBlockFlow* block_flow = ToLayoutBlockFlow(block);
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
  LayoutInline* continuation = InlineElementContinuation();
  LayoutInline* end_of_continuation = nullptr;
  for (LayoutInline* curr_cont = continuation; curr_cont;
       curr_cont = curr_cont->InlineElementContinuation()) {
    LayoutBoxModelObject* next_cont = curr_cont->Continuation();
    curr_cont->SetContinuation(nullptr);
    curr_cont->SetStyle(MutableStyle());
    curr_cont->SetContinuation(next_cont);
    end_of_continuation = curr_cont;
  }

  if (continuation && old_style) {
    DCHECK(end_of_continuation);
    LayoutObject* block = ContainingBlock()->NextSibling();
    // If an inline's in-flow positioning has changed then any descendant blocks
    // will need to change their styles accordingly.
    if (block && block->IsAnonymousBlock() &&
        new_style.GetPosition() != old_style->GetPosition() &&
        (new_style.HasInFlowPosition() || old_style->HasInFlowPosition()))
      UpdateInFlowPositionOfAnonymousBlockContinuations(
          block, new_style, *old_style, end_of_continuation->ContainingBlock());
  }

  if (!AlwaysCreateLineBoxes()) {
    bool always_create_line_boxes_new =
        HasSelfPaintingLayer() || HasBoxDecorationBackground() ||
        new_style.HasPadding() || new_style.HasMargin() ||
        new_style.HasOutline();
    if (old_style && always_create_line_boxes_new) {
      DirtyLineBoxes(false);
      SetNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::kStyleChange);
    }
    SetAlwaysCreateLineBoxes(always_create_line_boxes_new);
  }

  // If we are changing to/from static, we need to reposition
  // out-of-flow positioned descendants.
  if (old_style && old_style->GetPosition() != new_style.GetPosition() &&
      (new_style.GetPosition() == EPosition::kStatic ||
       old_style->GetPosition() == EPosition::kStatic)) {
    LayoutBlock* abs_containing_block = nullptr;
    if (old_style->GetPosition() == EPosition::kStatic) {
      abs_containing_block = ContainingBlockForAbsolutePosition();
    } else {
      // When position was not static, containingBlockForAbsolutePosition
      // for our children is our existing containingBlock.
      abs_containing_block = ContainingBlock();
    }
    if (abs_containing_block)
      abs_containing_block->RemovePositionedObjects(this, kNewContainingBlock);
  }

  PropagateStyleToAnonymousChildren();
}

void LayoutInline::UpdateAlwaysCreateLineBoxes(bool full_layout) {
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
      Style()->VerticalAlign() != EVerticalAlign::kBaseline ||
      Style()->GetTextEmphasisMark() != TextEmphasisMark::kNone ||
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
  return ToLayoutBlockFlow(layout_object)->InlineElementContinuation();
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
    // We are placing a block inside an inline. We have to perform a split of
    // this inline into continuations. This involves creating an anonymous
    // block box to hold |newChild|. We then make that block box a continuation
    // of this inline. We take all of the children after |beforeChild| and put
    // them in a clone of this object.
    scoped_refptr<ComputedStyle> new_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(StyleRef(),
                                                       EDisplay::kBlock);
    // The anon block we create here doesn't exist in the CSS spec, so
    // we need to ensure that any blocks it contains inherit properly
    // from its true parent. This means they must use the direction set by the
    // anon block's containing block, so we need to prevent the anon block
    // from inheriting direction from the inline. If there are any other
    // inheritable properties that apply to block and inline elements
    // but only affect the layout of children we will want to special-case
    // them here too. Writing-mode would be one if it didn't create a
    // formatting context of its own, removing the need for continuations.
    new_style->SetDirection(ContainingBlock()->StyleRef().Direction());

    // If inside an inline affected by in-flow positioning the block needs to be
    // affected by it too. Giving the block a layer like this allows it to
    // collect the x/y offsets from inline parents later.
    if (LayoutObject* positioned_ancestor =
            InFlowPositionedInlineAncestor(this))
      new_style->SetPosition(positioned_ancestor->Style()->GetPosition());

    LayoutBlockFlow* new_box = LayoutBlockFlow::CreateAnonymous(&GetDocument());
    new_box->SetStyle(std::move(new_style));
    LayoutBoxModelObject* old_continuation = Continuation();
    SetContinuation(new_box);

    SplitFlow(before_child, new_box, new_child, old_continuation);
    return;
  }

  LayoutBoxModelObject::AddChild(new_child, before_child);

  new_child->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kChildChanged);
}

LayoutInline* LayoutInline::Clone() const {
  LayoutInline* clone_inline = new LayoutInline(GetNode());
  DCHECK(!IsAnonymous());
  clone_inline->SetStyle(MutableStyle());
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

  // If we're splitting the inline containing the fullscreened element,
  // |beforeChild| may be the layoutObject for the fullscreened element.
  // However, that layoutObject is wrapped in a LayoutFullScreen, so |this| is
  // not its parent. Since the splitting logic expects |this| to be the parent,
  // set |beforeChild| to be the LayoutFullScreen.
  if (Fullscreen* fullscreen = Fullscreen::FromIfExists(GetDocument())) {
    const Element* fullscreen_element = fullscreen->FullscreenElement();
    if (fullscreen_element && before_child &&
        before_child->GetNode() == fullscreen_element)
      before_child = fullscreen->FullScreenLayoutObject();
  }

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
  LayoutBlockFlow* block = ToLayoutBlockFlow(ContainingBlock());
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
      block = ToLayoutBlockFlow(outer_containing_block);
      reused_anonymous_block = true;
    }
  }

  // No anonymous block available for use. Make one.
  if (!reused_anonymous_block)
    pre = ToLayoutBlockFlow(block->CreateAnonymousBlock());

  LayoutBlockFlow* post = ToLayoutBlockFlow(pre->CreateAnonymousBlock());

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
      no->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::kAnonymousBlockChange);
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
  pre->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kAnonymousBlockChange);
  block->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kAnonymousBlockChange);
  post->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kAnonymousBlockChange);
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

void LayoutInline::Paint(const PaintInfo& paint_info,
                         const LayoutPoint& paint_offset) const {
  if (RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled()) {
    if (LayoutBlockFlow* block_flow = EnclosingNGBlockFlow()) {
      if (NGPaintFragment* block_flow_fragment = block_flow->PaintFragment()) {
        block_flow_fragment->PaintInlineBoxForDescendants(paint_info,
                                                          paint_offset, this);
        return;
      }
    }
  }

  InlinePainter(*this).Paint(paint_info, paint_offset);
}

template <typename GeneratorContext>
void LayoutInline::GenerateLineBoxRects(GeneratorContext& yield) const {
  if (!AlwaysCreateLineBoxes()) {
    GenerateCulledLineBoxRects(yield, this);
  } else if (InlineFlowBox* curr = FirstLineBox()) {
    for (; curr; curr = curr->NextLineBox())
      yield(LayoutRect(curr->Location(), curr->Size()));
  }
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

template <typename GeneratorContext>
void LayoutInline::GenerateCulledLineBoxRects(
    GeneratorContext& yield,
    const LayoutInline* container) const {
  if (!CulledInlineFirstLineBox())
    return;

  bool is_horizontal = Style()->IsHorizontalWritingMode();

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
        curr_inline->GenerateCulledLineBoxRects(yield, container);
      } else {
        for (InlineFlowBox* child_line = curr_inline->FirstLineBox();
             child_line; child_line = child_line->NextLineBox()) {
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
      for (InlineTextBox* child_text : InlineTextBoxesOf(*curr_text)) {
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

namespace {

class AbsoluteRectsGeneratorContext {
 public:
  AbsoluteRectsGeneratorContext(Vector<IntRect>& rects,
                                const LayoutPoint& accumulated_offset)
      : rects_(rects), accumulated_offset_(accumulated_offset) {}

  void operator()(const LayoutRect& rect) {
    IntRect int_rect = EnclosingIntRect(rect);
    int_rect.Move(accumulated_offset_.X().ToInt(),
                  accumulated_offset_.Y().ToInt());
    rects_.push_back(int_rect);
  }

 private:
  Vector<IntRect>& rects_;
  const LayoutPoint& accumulated_offset_;
};

}  // unnamed namespace

void LayoutInline::AbsoluteRects(Vector<IntRect>& rects,
                                 const LayoutPoint& accumulated_offset) const {
  AbsoluteRectsGeneratorContext context(rects, accumulated_offset);
  GenerateLineBoxRects(context);
  if (rects.IsEmpty())
    context(LayoutRect());

  if (const LayoutBoxModelObject* continuation = Continuation()) {
    if (continuation->IsBox()) {
      const LayoutBox* box = ToLayoutBox(continuation);
      continuation->AbsoluteRects(
          rects,
          ToLayoutPoint(accumulated_offset - ContainingBlock()->Location() +
                        box->LocationOffset()));
    } else {
      continuation->AbsoluteRects(
          rects,
          ToLayoutPoint(accumulated_offset - ContainingBlock()->Location()));
    }
  }
}

namespace {

class AbsoluteQuadsGeneratorContext {
 public:
  AbsoluteQuadsGeneratorContext(const LayoutInline* layout_object,
                                Vector<FloatQuad>& quads,
                                MapCoordinatesFlags mode)
      : quads_(quads), geometry_map_(mode) {
    geometry_map_.PushMappingsToAncestor(layout_object, nullptr);
  }

  void operator()(const FloatRect& rect) {
    quads_.push_back(geometry_map_.AbsoluteRect(rect));
  }
  void operator()(const LayoutRect& rect) { operator()(FloatRect(rect)); }

 private:
  Vector<FloatQuad>& quads_;
  LayoutGeometryMap geometry_map_;
};

}  // unnamed namespace

void LayoutInline::AbsoluteQuadsForSelf(Vector<FloatQuad>& quads,
                                        MapCoordinatesFlags mode) const {
  AbsoluteQuadsGeneratorContext context(this, quads, mode);
  GenerateLineBoxRects(context);
  if (quads.IsEmpty())
    context(FloatRect());
}

LayoutPoint LayoutInline::FirstLineBoxTopLeft() const {
  if (InlineBox* first_box = FirstLineBoxIncludingCulling())
    return first_box->Location();
  return LayoutPoint();
}

LayoutUnit LayoutInline::OffsetLeft(const Element* parent) const {
  return AdjustedPositionRelativeTo(FirstLineBoxTopLeft(), parent).X();
}

LayoutUnit LayoutInline::OffsetTop(const Element* parent) const {
  return AdjustedPositionRelativeTo(FirstLineBoxTopLeft(), parent).Y();
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
  return ComputeMargin(this, Style()->MarginLeft());
}

LayoutUnit LayoutInline::MarginRight() const {
  return ComputeMargin(this, Style()->MarginRight());
}

LayoutUnit LayoutInline::MarginTop() const {
  return ComputeMargin(this, Style()->MarginTop());
}

LayoutUnit LayoutInline::MarginBottom() const {
  return ComputeMargin(this, Style()->MarginBottom());
}

bool LayoutInline::NodeAtPoint(HitTestResult& result,
                               const HitTestLocation& location_in_container,
                               const LayoutPoint& accumulated_offset,
                               HitTestAction hit_test_action) {
  return line_boxes_.HitTest(LineLayoutBoxModel(this), result,
                             location_in_container, accumulated_offset,
                             hit_test_action);
}

namespace {

class HitTestCulledInlinesGeneratorContext {
 public:
  HitTestCulledInlinesGeneratorContext(Region& region,
                                       const HitTestLocation& location)
      : intersected_(false), region_(region), location_(location) {}
  void operator()(const FloatRect& rect) {
    if (location_.Intersects(rect)) {
      intersected_ = true;
      region_.Unite(EnclosingIntRect(rect));
    }
  }
  void operator()(const LayoutRect& rect) {
    if (location_.Intersects(rect)) {
      intersected_ = true;
      region_.Unite(EnclosingIntRect(rect));
    }
  }
  bool Intersected() const { return intersected_; }

 private:
  bool intersected_;
  Region& region_;
  const HitTestLocation& location_;
};

}  // unnamed namespace

bool LayoutInline::HitTestCulledInline(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset) {
  DCHECK(!AlwaysCreateLineBoxes());
  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  HitTestLocation adjusted_location(location_in_container,
                                    -ToLayoutSize(accumulated_offset));

  Region region_result;
  HitTestCulledInlinesGeneratorContext context(region_result,
                                               adjusted_location);
  GenerateCulledLineBoxRects(context, this);

  if (context.Intersected()) {
    UpdateHitTestResult(result, adjusted_location.Point());
    if (result.AddNodeToListBasedTestResult(GetNode(), adjusted_location,
                                            region_result) == kStopHitTesting)
      return true;
  }
  return false;
}

PositionWithAffinity LayoutInline::PositionForPoint(const LayoutPoint& point) {
  // FIXME: Does not deal with relative positioned inlines (should it?)

  // If there are continuations, test them first because our containing block
  // will not check them.
  LayoutBoxModelObject* continuation = Continuation();
  while (continuation) {
    if (continuation->IsInline() || continuation->SlowFirstChild())
      return continuation->PositionForPoint(point);
    continuation = ToLayoutBlockFlow(continuation)->InlineElementContinuation();
  }

  if (FirstLineBoxIncludingCulling()) {
    // This inline actually has a line box.  We must have clicked in the
    // border/padding of one of these boxes.  We
    // should try to find a result by asking our containing block.
    return ContainingBlock()->PositionForPoint(point);
  }

  return LayoutBoxModelObject::PositionForPoint(point);
}

namespace {

class LinesBoundingBoxGeneratorContext {
 public:
  LinesBoundingBoxGeneratorContext(FloatRect& rect) : rect_(rect) {}
  void operator()(const FloatRect& rect) { rect_.UniteIfNonZero(rect); }
  void operator()(const LayoutRect& rect) { operator()(FloatRect(rect)); }

 private:
  FloatRect& rect_;
};

}  // unnamed namespace

LayoutRect LayoutInline::LinesBoundingBox() const {
  if (const NGPhysicalBoxFragment* box_fragment =
          EnclosingBlockFlowFragmentOf(*this)) {
    LayoutRect result;
    auto children =
        NGInlineFragmentTraversal::SelfFragmentsOf(*box_fragment, this);
    for (const auto& child : children) {
      NGPhysicalOffset left_top =
          child.fragment->Offset() + child.offset_to_container_box;
      result.Unite(LayoutRect(LayoutPoint(left_top.left, left_top.top),
                              child.fragment->Size().ToLayoutSize()));
    }
    return result;
  }

  if (!AlwaysCreateLineBoxes()) {
    DCHECK(!FirstLineBox());
    FloatRect float_result;
    LinesBoundingBoxGeneratorContext context(float_result);
    GenerateCulledLineBoxRects(context, this);
    return EnclosingLayoutRect(float_result);
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
    for (InlineFlowBox* curr = FirstLineBox(); curr;
         curr = curr->NextLineBox()) {
      if (curr == FirstLineBox() || curr->LogicalLeft() < logical_left_side)
        logical_left_side = curr->LogicalLeft();
      if (curr == FirstLineBox() || curr->LogicalRight() > logical_right_side)
        logical_right_side = curr->LogicalRight();
    }

    bool is_horizontal = Style()->IsHorizontalWritingMode();

    LayoutUnit x = is_horizontal ? logical_left_side : FirstLineBox()->X();
    LayoutUnit y = is_horizontal ? FirstLineBox()->Y() : logical_left_side;
    LayoutUnit width = is_horizontal ? logical_right_side - logical_left_side
                                     : LastLineBox()->LogicalBottom() - x;
    LayoutUnit height = is_horizontal ? LastLineBox()->LogicalBottom() - y
                                      : logical_right_side - logical_left_side;
    result = LayoutRect(x, y, width, height);
  }

  return result;
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

LayoutRect LayoutInline::CulledInlineVisualOverflowBoundingBox() const {
  FloatRect float_result;
  LinesBoundingBoxGeneratorContext context(float_result);
  GenerateCulledLineBoxRects(context, this);
  LayoutRect result(EnclosingLayoutRect(float_result));
  bool is_horizontal = Style()->IsHorizontalWritingMode();
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
          result.UniteIfNonZero(logical_rect);
        } else {
          logical_rect.MoveBy(curr_box->Location());
          result.UniteIfNonZero(logical_rect.TransposedRect());
        }
      }
    } else if (curr->IsLayoutInline()) {
      // If the child doesn't need line boxes either, then we can recur.
      LayoutInline* curr_inline = ToLayoutInline(curr);
      if (!curr_inline->AlwaysCreateLineBoxes())
        result.UniteIfNonZero(
            curr_inline->CulledInlineVisualOverflowBoundingBox());
      else if (!curr_inline->HasSelfPaintingLayer())
        result.UniteIfNonZero(curr_inline->VisualOverflowRect());
    } else if (curr->IsText()) {
      LayoutText* curr_text = ToLayoutText(curr);
      result.UniteIfNonZero(curr_text->VisualOverflowRect());
    }
  }
  return result;
}

LayoutRect LayoutInline::LinesVisualOverflowBoundingBox() const {
  if (const NGPhysicalBoxFragment* box_fragment =
          EnclosingBlockFlowFragmentOf(*this)) {
    NGPhysicalOffsetRect result;
    auto children =
        NGInlineFragmentTraversal::SelfFragmentsOf(*box_fragment, this);
    for (const auto& child : children) {
      NGPhysicalOffsetRect child_rect =
          child.fragment->VisualRectWithContents();
      child_rect.offset += child.offset_to_container_box;
      result.Unite(child_rect);
    }
    return result.ToLayoutRect();
  }

  if (!AlwaysCreateLineBoxes())
    return CulledInlineVisualOverflowBoundingBox();

  if (!FirstLineBox() || !LastLineBox())
    return LayoutRect();

  // Return the width of the minimal left side and the maximal right side.
  LayoutUnit logical_left_side = LayoutUnit::Max();
  LayoutUnit logical_right_side = LayoutUnit::Min();
  for (InlineFlowBox* curr = FirstLineBox(); curr; curr = curr->NextLineBox()) {
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
  if (!Style()->IsHorizontalWritingMode())
    rect = rect.TransposedRect();
  return rect;
}

LayoutRect LayoutInline::AbsoluteVisualRect() const {
  if (!Continuation()) {
    LayoutRect rect = VisualOverflowRect();
    MapToVisualRectInAncestorSpace(View(), rect);
    return rect;
  }

  FloatRect float_result;
  LinesBoundingBoxGeneratorContext context(float_result);

  LayoutInline* end_continuation = InlineElementContinuation();
  while (LayoutInline* next_continuation =
             end_continuation->InlineElementContinuation())
    end_continuation = next_continuation;

  for (LayoutBlock* curr_block = ContainingBlock();
       curr_block && curr_block->IsAnonymousBlock();
       curr_block = ToLayoutBlock(curr_block->NextSibling())) {
    bool walk_children_only = !curr_block->ChildrenInline();
    for (LayoutObject* curr = curr_block->FirstChild(); curr;
         curr = curr->NextSibling()) {
      LayoutRect rect(curr->LocalVisualRect());
      context(FloatRect(rect));
      if (walk_children_only)
        continue;
      for (LayoutObject* walker = curr; walker;
           walker = walker->NextInPreOrder(curr)) {
        if (walker != end_continuation)
          continue;
        LayoutRect rect(EnclosingIntRect(float_result));
        MapToVisualRectInAncestorSpace(View(), rect);
        return rect;
      }
    }
  }
  return LayoutRect();
}

LayoutRect LayoutInline::LocalVisualRectIgnoringVisibility() const {
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    NGPhysicalOffsetRect visual_rect;
    if (LayoutNGBlockFlow::LocalVisualRectFor(this, &visual_rect))
      return visual_rect.ToLayoutRect();
  }

  // If we don't create line boxes, we don't have any invalidations to do.
  if (!AlwaysCreateLineBoxes())
    return LayoutRect();

  return VisualOverflowRect();
}

LayoutRect LayoutInline::VisualOverflowRect() const {
  LayoutRect overflow_rect = LinesVisualOverflowBoundingBox();
  LayoutUnit outline_outset(Style()->OutlineOutsetExtent());
  if (outline_outset) {
    Vector<LayoutRect> rects;
    if (GetDocument().InNoQuirksMode()) {
      // We have already included outline extents of line boxes in
      // linesVisualOverflowBoundingBox(), so the following just add outline
      // rects for children and continuations.
      AddOutlineRectsForChildrenAndContinuations(
          rects, LayoutPoint(), OutlineRectsShouldIncludeBlockVisualOverflow());
    } else {
      // In non-standard mode, because the difference in
      // LayoutBlock::minLineHeightForReplacedObject(),
      // linesVisualOverflowBoundingBox() may not cover outline rects of lines
      // containing replaced objects.
      AddOutlineRects(rects, LayoutPoint(),
                      OutlineRectsShouldIncludeBlockVisualOverflow());
    }
    if (!rects.IsEmpty()) {
      LayoutRect outline_rect = UnionRectEvenIfEmpty(rects);
      outline_rect.Inflate(outline_outset);
      overflow_rect.Unite(outline_rect);
    }
  }
  return overflow_rect;
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

  bool preserve3d = container->Style()->Preserves3D();

  TransformState::TransformAccumulation accumulation =
      preserve3d ? TransformState::kAccumulateTransform
                 : TransformState::kFlattenTransform;

  if (Style()->HasInFlowPosition() && Layer()) {
    // Apply the in-flow position offset when invalidating a rectangle. The
    // layer is translated, but the layout box isn't, so we need to do this to
    // get the right dirty rect. Since this is called from LayoutObject::
    // setStyle, the relative position flag on the LayoutObject has been
    // cleared, so use the one on the style().
    transform_state.Move(Layer()->OffsetForInFlowPosition(), accumulation);
  }

  LayoutBox* container_box =
      container->IsBox() ? ToLayoutBox(container) : nullptr;
  if (container_box && container != ancestor &&
      !container_box->MapScrollingContentsRectToBoxSpace(
          transform_state, accumulation, visual_rect_flags))
    return false;

  // TODO(wkorman): Generalize Ruby specialization and/or document more clearly.
  if (container_box && !IsRuby()) {
    transform_state.Flatten();
    LayoutRect rect(transform_state.LastPlanarQuad().BoundingBox());
    container_box->FlipForWritingMode(rect);
    transform_state.SetQuad(FloatQuad(FloatRect(rect)));
  }
  return container->MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

LayoutSize LayoutInline::OffsetFromContainer(
    const LayoutObject* container) const {
  DCHECK_EQ(container, Container());

  LayoutSize offset;
  if (IsInFlowPositioned())
    offset += OffsetForInFlowPosition();

  if (container->HasOverflowClip())
    offset -= ToLayoutBox(container)->ScrolledContentOffset();

  return offset;
}

PaintLayerType LayoutInline::LayerTypeRequired() const {
  return IsInFlowPositioned() || CreatesGroup() || HasClipPath() ||
                 Style()->ShouldCompositeForCurrentAnimations() ||
                 Style()->ContainsPaint()
             ? kNormalPaintLayer
             : kNoPaintLayer;
}

void LayoutInline::ChildBecameNonInline(LayoutObject* child) {
  // We have to split the parent flow.
  LayoutBlockFlow* new_box =
      ToLayoutBlockFlow(ContainingBlock()->CreateAnonymousBlock());
  LayoutBoxModelObject* old_continuation = Continuation();
  SetContinuation(new_box);
  LayoutObject* before_child = child->NextSibling();
  Children()->RemoveChildNode(this, child);
  SplitFlow(before_child, new_box, child, old_continuation);
}

void LayoutInline::UpdateHitTestResult(HitTestResult& result,
                                       const LayoutPoint& point) {
  if (result.InnerNode())
    return;

  Node* n = GetNode();
  LayoutPoint local_point(point);
  if (n) {
    if (IsInlineElementContinuation()) {
      // We're in the continuation of a split inline. Adjust our local point to
      // be in the coordinate space of the principal layoutObject's containing
      // block. This will end up being the innerNode.
      LayoutBlock* first_block = n->GetLayoutObject()->ContainingBlock();

      // Get our containing block.
      LayoutBox* block = ContainingBlock();
      local_point.MoveBy(block->Location() - first_block->LocationOffset());
    }

    result.SetNodeAndPosition(n, local_point);
  }
}

void LayoutInline::DirtyLineBoxes(bool full_layout) {
  if (full_layout) {
    line_boxes_.DeleteLineBoxes();
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
          for (InlineFlowBox* child_line = curr_inline->FirstLineBox();
               child_line; child_line = child_line->NextLineBox())
            child_line->Root().MarkDirty();
        } else if (curr->IsText()) {
          LayoutText* curr_text = ToLayoutText(curr);
          for (InlineTextBox* child_text : InlineTextBoxesOf(*curr_text))
            child_text->Root().MarkDirty();
        }
      }
    }
  } else {
    line_boxes_.DirtyLineBoxes();
  }
}

InlineFlowBox* LayoutInline::CreateInlineFlowBox() {
  return new InlineFlowBox(LineLayoutItem(this));
}

InlineFlowBox* LayoutInline::CreateAndAppendInlineFlowBox() {
  SetAlwaysCreateLineBoxes();
  InlineFlowBox* flow_box = CreateInlineFlowBox();
  line_boxes_.AppendLineBox(flow_box);
  return flow_box;
}

void LayoutInline::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  // During layout tree construction, we can't detect whether this node is
  // in LayoutNG or not.
  if (Parent() && EnclosingNGBlockFlow()) {
    SetAncestorLineBoxDirty();
    return;
  }
  line_boxes_.DirtyLinesFromChangedChild(
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

  return LayoutUnit(Style()->ComputedLineHeight());
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

LayoutSize LayoutInline::OffsetForInFlowPositionedInline(
    const LayoutBox& child) const {
  // FIXME: This function isn't right with mixed writing modes.

  DCHECK(IsInFlowPositioned());
  if (!IsInFlowPositioned())
    return LayoutSize();

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
    inline_position = Layer()->StaticInlinePosition();
    block_position = Layer()->StaticBlockPosition();
  }

  // Per http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width an
  // absolute positioned box with a static position should locate itself as
  // though it is a normal flow box in relation to its containing block. If this
  // relative-positioned inline has a negative offset we need to compensate for
  // it so that we align the positioned object with the edge of its containing
  // block.
  if (child.Style()->HasStaticInlinePosition(
          Style()->IsHorizontalWritingMode()))
    logical_offset.SetWidth(
        std::max(LayoutUnit(), -OffsetForInFlowPosition().Width()));
  else
    logical_offset.SetWidth(inline_position);

  if (!child.Style()->HasStaticBlockPosition(
          Style()->IsHorizontalWritingMode()))
    logical_offset.SetHeight(block_position);

  return Style()->IsHorizontalWritingMode() ? logical_offset
                                            : logical_offset.TransposedSize();
}

void LayoutInline::ImageChanged(WrappedImagePtr,
                                CanDeferInvalidation,
                                const IntRect*) {
  if (!Parent())
    return;

  // FIXME: We can do better.
  SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kImage);
}

namespace {

class AbsoluteLayoutRectsGeneratorContext {
 public:
  AbsoluteLayoutRectsGeneratorContext(Vector<LayoutRect>& rects,
                                      const LayoutPoint& accumulated_offset)
      : rects_(rects), accumulated_offset_(accumulated_offset) {}

  void operator()(const FloatRect& rect) { operator()(LayoutRect(rect)); }
  void operator()(const LayoutRect& rect) {
    LayoutRect layout_rect(rect);
    layout_rect.MoveBy(accumulated_offset_);
    rects_.push_back(layout_rect);
  }

 private:
  Vector<LayoutRect>& rects_;
  const LayoutPoint& accumulated_offset_;
};

}  // unnamed namespace

void LayoutInline::AddOutlineRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    IncludeBlockVisualOverflowOrNot include_block_overflows) const {
  AbsoluteLayoutRectsGeneratorContext context(rects, additional_offset);
  GenerateLineBoxRects(context);
  AddOutlineRectsForChildrenAndContinuations(rects, additional_offset,
                                             include_block_overflows);
}

void LayoutInline::AddOutlineRectsForChildrenAndContinuations(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    IncludeBlockVisualOverflowOrNot include_block_overflows) const {
  AddOutlineRectsForNormalChildren(rects, additional_offset,
                                   include_block_overflows);
  AddOutlineRectsForContinuations(rects, additional_offset,
                                  include_block_overflows);
}

void LayoutInline::AddOutlineRectsForContinuations(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    IncludeBlockVisualOverflowOrNot include_block_overflows) const {
  if (LayoutBoxModelObject* continuation = Continuation()) {
    if (continuation->IsInline())
      continuation->AddOutlineRects(
          rects,
          additional_offset + (continuation->ContainingBlock()->Location() -
                               ContainingBlock()->Location()),
          include_block_overflows);
    else
      continuation->AddOutlineRects(
          rects,
          additional_offset + (ToLayoutBox(continuation)->Location() -
                               ContainingBlock()->Location()),
          include_block_overflows);
  }
}

FloatRect LayoutInline::LocalBoundingBoxRectForAccessibility() const {
  Vector<LayoutRect> rects;
  AddOutlineRects(rects, LayoutPoint(), kIncludeBlockVisualOverflow);
  return FloatRect(UnionRect(rects));
}

void LayoutInline::ComputeSelfHitTestRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint& layer_offset) const {
  AbsoluteLayoutRectsGeneratorContext context(rects, layer_offset);
  GenerateLineBoxRects(context);
}

void LayoutInline::AddAnnotatedRegions(Vector<AnnotatedRegionValue>& regions) {
  // Convert the style regions to absolute coordinates.
  if (Style()->Visibility() != EVisibility::kVisible)
    return;

  if (Style()->DraggableRegionMode() == EDraggableRegionMode::kNone)
    return;

  AnnotatedRegionValue region;
  region.draggable =
      Style()->DraggableRegionMode() == EDraggableRegionMode::kDrag;
  region.bounds = LayoutRect(LinesBoundingBox());

  LayoutObject* container = ContainingBlock();
  if (!container)
    container = this;

  FloatPoint abs_pos = container->LocalToAbsolute();
  region.bounds.SetX(LayoutUnit(abs_pos.X() + region.bounds.X()));
  region.bounds.SetY(LayoutUnit(abs_pos.Y() + region.bounds.Y()));

  regions.push_back(region);
}

void LayoutInline::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  // TODO(yoichio): Cover other PaintInvalidateionReasons.
  DCHECK(invalidation_reason != PaintInvalidationReason::kSelection ||
         !EnclosingNGBlockFlow());
  ObjectPaintInvalidator paint_invalidator(*this);
  paint_invalidator.InvalidateDisplayItemClient(*this, invalidation_reason);

  for (InlineFlowBox* box = FirstLineBox(); box; box = box->NextLineBox())
    paint_invalidator.InvalidateDisplayItemClient(*box, invalidation_reason);
}

// TODO(loonybear): Not to just dump 0, 0 as the x and y here
LayoutRect LayoutInline::DebugRect() const {
  IntRect lines_box = EnclosingIntRect(LinesBoundingBox());
  return LayoutRect(IntRect(0, 0, lines_box.Width(), lines_box.Height()));
}

}  // namespace blink

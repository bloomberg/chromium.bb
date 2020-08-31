// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fieldset_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_mathml_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/core/paint/url_metadata_utils.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"

namespace blink {

namespace {

LayoutRectOutsets BoxStrutToLayoutRectOutsets(
    const NGPixelSnappedPhysicalBoxStrut& box_strut) {
  return LayoutRectOutsets(
      LayoutUnit(box_strut.top), LayoutUnit(box_strut.right),
      LayoutUnit(box_strut.bottom), LayoutUnit(box_strut.left));
}

inline bool HasSelection(const LayoutObject* layout_object) {
  return layout_object->GetSelectionState() != SelectionState::kNone;
}

inline bool IsVisibleToPaint(const NGPhysicalFragment& fragment,
                             const ComputedStyle& style) {
  return !fragment.IsHiddenForPaint() &&
         style.Visibility() == EVisibility::kVisible;
}

inline bool IsVisibleToPaint(const NGFragmentItem& item,
                             const ComputedStyle& style) {
  return !item.IsHiddenForPaint() &&
         style.Visibility() == EVisibility::kVisible;
}

inline bool IsVisibleToHitTest(const HitTestRequest& request,
                               const ComputedStyle& style) {
  return request.IgnorePointerEventsNone() ||
         style.PointerEvents() != EPointerEvents::kNone;
}

inline bool IsVisibleToHitTest(const NGFragmentItem& item,
                               const HitTestRequest& request) {
  const ComputedStyle& style = item.Style();
  return IsVisibleToPaint(item, style) && IsVisibleToHitTest(request, style);
}

bool FragmentVisibleToHitTestRequest(const NGPhysicalFragment& fragment,
                                     const HitTestRequest& request) {
  const ComputedStyle& style = fragment.Style();
  return IsVisibleToPaint(fragment, style) &&
         IsVisibleToHitTest(request, style);
}

// Hit tests inline ancestor elements of |fragment| who do not have their own
// box fragments.
// @param physical_offset Physical offset of |fragment| in the paint layer.
bool HitTestCulledInlineAncestors(
    HitTestResult& result,
    const NGInlineCursor& parent_cursor,
    const LayoutObject* current,
    const LayoutObject* limit,
    const NGInlineCursorPosition& previous_sibling,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset fallback_accumulated_offset) {
  DCHECK(current != limit && current->IsDescendantOf(limit));
  for (LayoutObject* parent = current->Parent(); parent && parent != limit;
       current = parent, parent = parent->Parent()) {
    // |culled_parent| is a culled inline element to be hit tested, since it's
    // "between" |fragment| and |fragment->Parent()| but doesn't have its own
    // box fragment.
    // To ensure the correct hit test ordering, |culled_parent| must be hit
    // tested only once after all of its descendants are hit tested:
    // - Shortcut: when |current_layout_object| is the only child (of
    // |culled_parent|), since it's just hit tested, we can safely hit test its
    // parent;
    // - General case: we hit test |culled_parent| only when it is not an
    // ancestor of |previous_sibling|; otherwise, |previous_sibling| has to be
    // hit tested first.
    // TODO(crbug.com/849331): It's wrong for bidi inline fragmentation. Fix it.
    const bool has_sibling =
        current->PreviousSibling() || current->NextSibling();
    if (has_sibling && previous_sibling &&
        previous_sibling.GetLayoutObject()->IsDescendantOf(parent))
      break;

    if (auto* parent_layout_inline = ToLayoutInlineOrNull(parent)) {
      if (parent_layout_inline->HitTestCulledInline(result, hit_test_location,
                                                    fallback_accumulated_offset,
                                                    &parent_cursor))
        return true;
    }
  }

  return false;
}

bool HitTestCulledInlineAncestors(
    HitTestResult& result,
    const NGInlineCursor& parent_cursor,
    const NGPaintFragment& fragment,
    const NGInlineCursorPosition& previous_sibling,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& physical_offset) {
  DCHECK(fragment.Parent());
  DCHECK(fragment.PhysicalFragment().IsInline());
  const NGPaintFragment& parent = *fragment.Parent();
  // To be passed as |accumulated_offset| to LayoutInline::HitTestCulledInline,
  // where it equals the physical offset of the containing block in paint layer.
  const PhysicalOffset fallback_accumulated_offset =
      physical_offset - fragment.OffsetInContainerBlock();
  const LayoutObject* limit_layout_object =
      parent.PhysicalFragment().IsLineBox() ? parent.Parent()->GetLayoutObject()
                                            : parent.GetLayoutObject();
  return HitTestCulledInlineAncestors(
      result, parent_cursor, fragment.GetLayoutObject(), limit_layout_object,
      previous_sibling, hit_test_location, fallback_accumulated_offset);
}

bool HitTestCulledInlineAncestors(
    HitTestResult& result,
    const NGPhysicalBoxFragment& container,
    const NGInlineCursor& parent_cursor,
    const NGFragmentItem& item,
    const NGInlineCursorPosition& previous_sibling,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& physical_offset) {
  // To be passed as |accumulated_offset| to LayoutInline::HitTestCulledInline,
  // where it equals the physical offset of the containing block in paint layer.
  const PhysicalOffset fallback_accumulated_offset =
      physical_offset - item.OffsetInContainerBlock();
  return HitTestCulledInlineAncestors(
      result, parent_cursor, item.GetLayoutObject(),
      // Limit the traversal up to the container fragment, or its container if
      // the fragment is not a CSSBox.
      container.GetSelfOrContainerLayoutObject(), previous_sibling,
      hit_test_location, fallback_accumulated_offset);
}

// Returns if this fragment may not be laid out by LayoutNG.
//
// This function is for an optimization to skip a few virtual
// calls. When this is |false|, we know |LayoutObject::Paint()| calls
// |NGBoxFragmentPainter|, and that we can instantiate a child
// |NGBoxFragmentPainter| directly. All code should work without this.
//
// TODO(kojii): This may become more complicated when we use
// |NGBoxFragmentPainter| for all fragments, and we still want this
// oprimization.
bool FragmentRequiresLegacyFallback(const NGPhysicalFragment& fragment) {
  // Fallback to LayoutObject if this is a root of NG block layout.
  // If this box is for this painter, LayoutNGBlockFlow will call this back.
  // Otherwise it calls legacy painters.
  return fragment.IsFormattingContextRoot();
}

// Returns a vector of backplates that surround the paragraphs of text within
// line_boxes.
//
// This function traverses descendants of an inline formatting context in
// pre-order DFS and build up backplates behind inline text boxes, each split at
// the paragraph level. Store the results in paragraph_backplates.
Vector<PhysicalRect> BuildBackplate(NGInlineCursor* descendants,
                                    const PhysicalOffset& paint_offset) {
  // The number of consecutive forced breaks that split the backplate by
  // paragraph.
  static constexpr int kMaxConsecutiveLineBreaks = 2;

  struct Backplates {
    STACK_ALLOCATED();

   public:
    void AddTextRect(const PhysicalRect& box_rect) {
      if (consecutive_line_breaks >= kMaxConsecutiveLineBreaks) {
        // This is a paragraph point.
        paragraph_backplates.push_back(current_backplate);
        current_backplate = PhysicalRect();
      }
      consecutive_line_breaks = 0;

      current_backplate.Unite(box_rect);
    }

    void AddLineBreak() { consecutive_line_breaks++; }

    Vector<PhysicalRect> paragraph_backplates;
    PhysicalRect current_backplate;
    int consecutive_line_breaks = 0;
  } backplates;

  // Build up and paint backplates of all child inline text boxes. We are not
  // able to simply use the linebox rect to compute the backplate because the
  // backplate should only be painted for inline text and not for atomic
  // inlines.
  for (; *descendants; descendants->MoveToNext()) {
    if (const NGPaintFragment* child = descendants->CurrentPaintFragment()) {
      const NGPhysicalFragment& child_fragment = child->PhysicalFragment();
      if (child_fragment.IsHiddenForPaint() || child_fragment.IsFloating())
        continue;
      if (auto* text_fragment =
              DynamicTo<NGPhysicalTextFragment>(child_fragment)) {
        if (text_fragment->IsLineBreak()) {
          backplates.AddLineBreak();
          continue;
        }

        PhysicalRect box_rect(child->OffsetInContainerBlock() + paint_offset,
                              child->Size());
        backplates.AddTextRect(box_rect);
      }
      continue;
    }
    if (const NGFragmentItem* child_item = descendants->CurrentItem()) {
      if (child_item->IsHiddenForPaint())
        continue;
      if (child_item->IsText()) {
        if (child_item->IsLineBreak()) {
          backplates.AddLineBreak();
          continue;
        }

        PhysicalRect box_rect(
            child_item->OffsetInContainerBlock() + paint_offset,
            child_item->Size());
        backplates.AddTextRect(box_rect);
      }
      continue;
    }
    NOTREACHED();
  }

  if (!backplates.current_backplate.IsEmpty())
    backplates.paragraph_backplates.push_back(backplates.current_backplate);
  return backplates.paragraph_backplates;
}

bool HitTestAllPhasesInFragment(const NGPhysicalBoxFragment& fragment,
                                const HitTestLocation& hit_test_location,
                                PhysicalOffset accumulated_offset,
                                HitTestResult* result) {
  // Hit test all phases of inline blocks, inline tables, replaced elements and
  // non-positioned floats as if they created their own (pseudo- [1]) stacking
  // context. https://www.w3.org/TR/CSS22/zindex.html#painting-order
  //
  // [1] As if it creates a new stacking context, but any positioned descendants
  // and descendants which actually create a new stacking context should be
  // considered part of the parent stacking context, not this new one.

  if (!fragment.CanTraverse()) {
    return fragment.GetMutableLayoutObject()->HitTestAllPhases(
        *result, hit_test_location, accumulated_offset);
  }

  return NGBoxFragmentPainter(To<NGPhysicalBoxFragment>(fragment))
      .HitTestAllPhases(*result, hit_test_location, accumulated_offset);
}

bool NodeAtPointInFragment(const NGPhysicalBoxFragment& fragment,
                           const HitTestLocation& hit_test_location,
                           PhysicalOffset accumulated_offset,
                           HitTestAction action,
                           HitTestResult* result) {
  if (!fragment.CanTraverse()) {
    return fragment.GetMutableLayoutObject()->NodeAtPoint(
        *result, hit_test_location, accumulated_offset, action);
  }

  return NGBoxFragmentPainter(fragment).NodeAtPoint(*result, hit_test_location,
                                                    accumulated_offset, action);
}

// Return an ID for this fragmentainer, which is unique within the fragmentation
// context. We need to provide this ID when block-fragmenting, so that we can
// cache the painting of each individual fragment.
unsigned FragmentainerUniqueIdentifier(const NGPhysicalBoxFragment& fragment) {
  if (const auto* break_token = To<NGBlockBreakToken>(fragment.BreakToken()))
    return break_token->SequenceNumber() + 1;
  return 0;
}

}  // anonymous namespace

const NGBorderEdges& NGBoxFragmentPainter::BorderEdges() const {
  if (border_edges_.has_value())
    return *border_edges_;
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  border_edges_ = NGBorderEdges::FromPhysical(
      fragment.BorderEdges(), fragment.Style().GetWritingMode());
  return *border_edges_;
}

PhysicalRect NGBoxFragmentPainter::SelfInkOverflow() const {
  if (paint_fragment_)
    return paint_fragment_->SelfInkOverflow();
  if (box_item_)
    return box_item_->SelfInkOverflow();
  const NGPhysicalFragment& fragment = PhysicalFragment();
  DCHECK(fragment.IsBox() && !fragment.IsInlineBox());
  return ToLayoutBox(fragment.GetLayoutObject())
      ->PhysicalSelfVisualOverflowRect();
}

PhysicalRect NGBoxFragmentPainter::ContentsInkOverflow() const {
  if (const LayoutObject* layout_object = box_fragment_.GetLayoutObject())
    return ToLayoutBox(layout_object)->PhysicalContentsVisualOverflowRect();
  return box_fragment_.ContentsInkOverflow();
}

void NGBoxFragmentPainter::Paint(const PaintInfo& paint_info) {
  if (PhysicalFragment().IsPaintedAtomically() &&
      !box_fragment_.HasSelfPaintingLayer())
    PaintAllPhasesAtomically(paint_info);
  else
    PaintInternal(paint_info);
}

void NGBoxFragmentPainter::PaintInternal(const PaintInfo& paint_info) {
  // Avoid initialization of Optional ScopedPaintState::chunk_properties_
  // and ScopedPaintState::adjusted_paint_info_.
  STACK_UNINITIALIZED ScopedPaintState paint_state(box_fragment_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  PaintInfo& info = paint_state.MutablePaintInfo();
  PhysicalOffset paint_offset = paint_state.PaintOffset();
  PaintPhase original_phase = info.phase;

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  if (original_phase == PaintPhase::kForeground &&
      box_fragment_.GetLayoutObject()->IsBox()) {
    scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
        ToLayoutBox(*box_fragment_.GetLayoutObject()),
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }

  if (original_phase == PaintPhase::kOutline) {
    info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    // With CompositeAfterPaint we need to call PaintObject twice: once for the
    // background painting that does not scroll, and a second time for the
    // background painting that scrolls.
    // Without CompositeAfterPaint, this happens as the main graphics layer
    // paints the background, and then the scrolling contents graphics layer
    // paints the background.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      auto paint_location = ToLayoutBox(*box_fragment_.GetLayoutObject())
                                .GetBackgroundPaintLocation();
      if (!(paint_location & kBackgroundPaintInGraphicsLayer))
        info.SetSkipsBackground(true);
      PaintObject(info, paint_offset);
      info.SetSkipsBackground(false);

      if (paint_location & kBackgroundPaintInScrollingContents) {
        info.SetIsPaintingScrollingBackground(true);
        PaintObject(info, paint_offset);
        info.SetIsPaintingScrollingBackground(false);
      }
    } else {
      PaintObject(info, paint_offset);
    }
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly &&
      original_phase != PaintPhase::kOverlayOverflowControls) {
    if (original_phase == PaintPhase::kMask ||
        !box_fragment_.GetLayoutObject()->IsBox()) {
      PaintObject(info, paint_offset);
    } else {
      ScopedBoxContentsPaintState contents_paint_state(
          paint_state, ToLayoutBox(*box_fragment_.GetLayoutObject()));
      PaintObject(contents_paint_state.GetPaintInfo(),
                  contents_paint_state.PaintOffset());
    }
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    info.phase = PaintPhase::kSelfOutlineOnly;
    PaintObject(info, paint_offset);
  }

  // We paint scrollbars after we painted other things, so that the scrollbars
  // will sit above them.
  info.phase = original_phase;
  if (box_fragment_.HasOverflowClip()) {
    ScrollableAreaPainter(*PhysicalFragment().Layer()->GetScrollableArea())
        .PaintOverflowControls(info, RoundedIntPoint(paint_offset));
  }
}

void NGBoxFragmentPainter::RecordScrollHitTestData(
    const PaintInfo& paint_info,
    const DisplayItemClient& background_client) {
  if (!box_fragment_.GetLayoutObject()->IsBox())
    return;
  BoxPainter(ToLayoutBox(*box_fragment_.GetLayoutObject()))
      .RecordScrollHitTestData(paint_info, background_client);
}

bool NGBoxFragmentPainter::ShouldRecordHitTestData(
    const PaintInfo& paint_info) {
  // Hit test data are only needed for compositing. This flag is used for for
  // printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return false;

  // If an object is not visible, it does not participate in hit testing.
  if (PhysicalFragment().Style().Visibility() != EVisibility::kVisible)
    return false;

  return true;
}

void NGBoxFragmentPainter::PaintObject(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    bool suppress_box_decoration_background) {
  const PaintPhase paint_phase = paint_info.phase;
  const NGPhysicalBoxFragment& physical_box_fragment = PhysicalFragment();
  const ComputedStyle& style = box_fragment_.Style();
  bool is_visible = IsVisibleToPaint(physical_box_fragment, style);
  if (!is_visible)
    suppress_box_decoration_background = true;

  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    PaintBoxDecorationBackground(paint_info, paint_offset,
                                 suppress_box_decoration_background);
    // We're done. We don't bother painting any children.
    if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_phase == PaintPhase::kMask && is_visible) {
    PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_phase == PaintPhase::kForeground) {
    if (paint_info.ShouldAddUrlMetadata()) {
      NGFragmentPainter(box_fragment_, GetDisplayItemClient())
          .AddURLRectIfNeeded(paint_info, paint_offset);
    }
    if (is_visible && box_fragment_.IsMathMLFraction())
      NGMathMLPainter(box_fragment_).PaintFractionBar(paint_info, paint_offset);
  }

  if (paint_phase != PaintPhase::kSelfOutlineOnly &&
      (!physical_box_fragment.Children().empty() ||
       physical_box_fragment.HasItems() || inline_box_cursor_) &&
      !paint_info.DescendantPaintingBlocked()) {
    if (UNLIKELY(paint_phase == PaintPhase::kForeground &&
                 box_fragment_.IsCSSBox() &&
                 box_fragment_.Style().HasColumnRule()))
      PaintColumnRules(paint_info, paint_offset);

    if (paint_phase != PaintPhase::kFloat) {
      if (UNLIKELY(inline_box_cursor_)) {
        // Use the descendants cursor for this painter if it is given.
        // Self-painting inline box paints only parts of the container block.
        // Adjust |paint_offset| because it is the offset of the inline box, but
        // |descendants_| has offsets to the contaiing block.
        DCHECK(box_item_);
        NGInlineCursor descendants = inline_box_cursor_->CursorForDescendants();
        const PhysicalOffset paint_offset_to_inline_formatting_context =
            paint_offset - box_item_->OffsetInContainerBlock();
        PaintInlineItems(paint_info.ForDescendants(),
                         paint_offset_to_inline_formatting_context,
                         box_item_->OffsetInContainerBlock(), &descendants);
      } else if (items_) {
        if (physical_box_fragment.IsBlockFlow()) {
          PaintBlockFlowContents(paint_info, paint_offset);
        } else {
          DCHECK(physical_box_fragment.IsInlineBox());
          NGInlineCursor cursor(*items_);
          PaintInlineItems(paint_info.ForDescendants(), paint_offset,
                           PhysicalOffset(), &cursor);
        }
      } else if (physical_box_fragment.IsInlineFormattingContext()) {
        DCHECK(!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
        DCHECK(paint_fragment_);
        if (physical_box_fragment.IsBlockFlow()) {
          PaintBlockFlowContents(paint_info, paint_offset);
        } else if (ShouldPaintDescendantOutlines(paint_info.phase)) {
          // TODO(kojii): |PaintInlineChildrenOutlines()| should do the work
          // instead. Legacy does so, and is more efficient. But NG outline
          // logic currently depends on |PaintInlineChildren()|.
          PaintInlineChildren(paint_fragment_->Children(),
                              paint_info.ForDescendants(), paint_offset);
        } else {
          PaintInlineChildren(paint_fragment_->Children(), paint_info,
                              paint_offset);
        }
      } else {
        PaintBlockChildren(paint_info, paint_offset);
      }
    }

    if (paint_phase == PaintPhase::kFloat ||
        paint_phase == PaintPhase::kSelectionDragImage ||
        paint_phase == PaintPhase::kTextClip) {
      if (physical_box_fragment.HasFloatingDescendantsForPaint())
        PaintFloats(paint_info);
    }
  }

  if (ShouldPaintSelfOutline(paint_phase)) {
    NGFragmentPainter(box_fragment_, GetDisplayItemClient())
        .PaintOutline(paint_info, paint_offset);
  }

  // If the caret's node's fragment's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  if (paint_phase == PaintPhase::kForeground &&
      physical_box_fragment.ShouldPaintCarets())
    PaintCarets(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintCarets(const PaintInfo& paint_info,
                                       const PhysicalOffset& paint_offset) {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  LocalFrame* frame = fragment.GetLayoutObject()->GetFrame();
  if (fragment.ShouldPaintCursorCaret())
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (fragment.ShouldPaintDragCaret()) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

void NGBoxFragmentPainter::PaintBlockFlowContents(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  DCHECK(fragment.IsInlineFormattingContext());

  // When the layout-tree gets into a bad state, we can end up trying to paint
  // a fragment with inline children, without a paint fragment. See:
  // http://crbug.com/1022545
  if ((!paint_fragment_ && !items_) ||
      (layout_object && layout_object->NeedsLayout())) {
    NOTREACHED();
    return;
  }

  // Trying to rule out a null GraphicsContext, see: https://crbug.com/1040298
  CHECK(&paint_info.context);

  // Check if there were contents to be painted and return early if none.
  // The union of |ContentsInkOverflow()| and |LocalRect()| covers the rect to
  // check, in both cases of:
  // 1. Painting non-scrolling contents.
  // 2. Painting scrolling contents.
  // For 1, check with |ContentsInkOverflow()|, except when there is no
  // overflow, in which case check with |LocalRect()|. For 2, check with
  // |LayoutOverflow()|, but this can be approximiated with
  // |ContentsInkOverflow()|.
  PhysicalRect content_ink_rect = fragment.LocalRect();
  content_ink_rect.Unite(ContentsInkOverflow());
  content_ink_rect.offset += PhysicalOffset(paint_offset);
  if (!paint_info.GetCullRect().Intersects(content_ink_rect.ToLayoutRect()))
    return;

  if (paint_fragment_) {
    NGInlineCursor children(*paint_fragment_);
    PaintLineBoxChildren(&children, paint_info.ForDescendants(), paint_offset);
    return;
  }
  DCHECK(items_);
  NGInlineCursor children(*items_);
  PaintLineBoxChildren(&children, paint_info.ForDescendants(), paint_offset);
}

void NGBoxFragmentPainter::PaintBlockChildren(const PaintInfo& paint_info,
                                              PhysicalOffset paint_offset) {
  DCHECK(!box_fragment_.IsInlineFormattingContext());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  for (const NGLink& child : box_fragment_.Children()) {
    const NGPhysicalFragment& child_fragment = *child;
    DCHECK(child_fragment.IsBox());
    if (child_fragment.HasSelfPaintingLayer() || child_fragment.IsFloating())
      continue;

    const auto& box_child_fragment = To<NGPhysicalBoxFragment>(child_fragment);
    if (box_child_fragment.CanTraverse()) {
      if (!box_child_fragment.GetLayoutObject()) {
        // It's normally FragmentData that provides us with the paint offset.
        // FragmentData is (at least currently) associated with a LayoutObject.
        // If we have no LayoutObject, we have no FragmentData, so we need to
        // calculate the offset on our own (which is very simple, anyway).
        // Bypass Paint() and jump directly to PaintObject(), to skip the code
        // that assumes that we have a LayoutObject (and FragmentData).
        PhysicalOffset child_offset = paint_offset + child.offset;

        if (box_child_fragment.IsColumnBox()) {
          // This is a fragmentainer, and when node inside a fragmentation
          // context paints multiple block fragments, we need to distinguish
          // between them somehow, for paint caching to work. Therefore,
          // establish a display item scope here.
          unsigned identifier =
              FragmentainerUniqueIdentifier(box_child_fragment);
          ScopedDisplayItemFragment scope(paint_info.context, identifier);
          NGBoxFragmentPainter(box_child_fragment)
              .PaintObject(paint_info, child_offset);
          continue;
        }

        NGBoxFragmentPainter(box_child_fragment)
            .PaintObject(paint_info, child_offset);
        continue;
      }

      NGBoxFragmentPainter(box_child_fragment)
          .Paint(paint_info_for_descendants);
      continue;
    }

    // Fall back to flow-thread painting when reaching a column (the flow thread
    // is treated as a self-painting PaintLayer when fragment traversal is
    // disabled, so nothing to do here).
    if (box_child_fragment.IsColumnBox())
      continue;

    auto* layout_object = child_fragment.GetLayoutObject();
    DCHECK(layout_object);
    if (child_fragment.IsPaintedAtomically() &&
        child_fragment.IsLegacyLayoutRoot()) {
      ObjectPainter(*layout_object)
          .PaintAllPhasesAtomically(paint_info_for_descendants);
    } else {
      // TODO(ikilpatrick): Once FragmentItem ships we should call the
      // NGBoxFragmentPainter directly for NG objects.
      layout_object->Paint(paint_info_for_descendants);
    }
  }
}

void NGBoxFragmentPainter::PaintFloatingItems(const PaintInfo& paint_info,
                                              NGInlineCursor* cursor) {
  for (; *cursor; cursor->MoveToNext()) {
    const NGFragmentItem* item = cursor->Current().Item();
    DCHECK(item);
    const NGPhysicalBoxFragment* child_fragment = item->BoxFragment();
    if (!child_fragment || child_fragment->HasSelfPaintingLayer() ||
        !child_fragment->IsFloating())
      continue;
    // TODO(kojii): The float is outside of the inline formatting context and
    // that it maybe another NG inline formatting context, NG block layout, or
    // legacy. NGBoxFragmentPainter can handle only the first case. In order
    // to cover more tests for other two cases, we always fallback to legacy,
    // which will forward back to NGBoxFragmentPainter if the float is for
    // NGBoxFragmentPainter. We can shortcut this for the first case when
    // we're more stable.
    ObjectPainter(*child_fragment->GetLayoutObject())
        .PaintAllPhasesAtomically(paint_info);
  }
}

void NGBoxFragmentPainter::PaintFloatingChildren(
    const NGPhysicalContainerFragment& container,
    const PaintInfo& paint_info,
    const PaintInfo& float_paint_info) {
  DCHECK(container.HasFloatingDescendantsForPaint());

  if (const NGPhysicalBoxFragment* box =
          DynamicTo<NGPhysicalBoxFragment>(&container)) {
    if (const NGFragmentItems* items = box->Items()) {
      NGInlineCursor cursor(*items);
      PaintFloatingItems(float_paint_info, &cursor);
      return;
    }
    if (inline_box_cursor_) {
      DCHECK(box->IsInlineBox());
      NGInlineCursor descendants = inline_box_cursor_->CursorForDescendants();
      PaintFloatingItems(float_paint_info, &descendants);
      return;
    }
    DCHECK(!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled() ||
           !box->IsInlineBox());
  }

  for (const NGLink& child : container.Children()) {
    const NGPhysicalFragment& child_fragment = *child;
    if (child_fragment.HasSelfPaintingLayer())
      continue;

    if (child_fragment.CanTraverse()) {
      if (child_fragment.IsFloating()) {
        NGBoxFragmentPainter(To<NGPhysicalBoxFragment>(child_fragment))
            .Paint(float_paint_info);
        continue;
      }

      // Any non-floated children which paint atomically shouldn't be traversed.
      if (child_fragment.IsPaintedAtomically())
        continue;
    } else {
      if (child_fragment.IsFloating()) {
        // TODO(kojii): The float is outside of the inline formatting context
        // and that it maybe another NG inline formatting context, NG block
        // layout, or legacy. NGBoxFragmentPainter can handle only the first
        // case. In order to cover more tests for other two cases, we always
        // fallback to legacy, which will forward back to NGBoxFragmentPainter
        // if the float is for NGBoxFragmentPainter. We can shortcut this for
        // the first case when we're more stable.

        ObjectPainter(*child_fragment.GetLayoutObject())
            .PaintAllPhasesAtomically(float_paint_info);
        continue;
      }

      // Any children which paint atomically shouldn't be traversed.
      if (child_fragment.IsPaintedAtomically())
        continue;

      if (child_fragment.Type() == NGPhysicalFragment::kFragmentBox &&
          FragmentRequiresLegacyFallback(child_fragment)) {
        child_fragment.GetLayoutObject()->Paint(paint_info);
        continue;
      }
    }

    // The selection paint traversal is special. We will visit all fragments
    // (including floats) in the normal paint traversal. There isn't any point
    // performing the special float traversal here.
    if (paint_info.phase == PaintPhase::kSelectionDragImage)
      continue;

    const auto* child_container =
        DynamicTo<NGPhysicalContainerFragment>(&child_fragment);
    if (!child_container || !child_container->HasFloatingDescendantsForPaint())
      continue;

    if (child_container->HasOverflowClip()) {
      // We need to properly visit this fragment for painting, rather than
      // jumping directly to its children (which is what we normally do when
      // looking for floats), in order to set up the clip rectangle.
      NGBoxFragmentPainter(To<NGPhysicalBoxFragment>(*child_container))
          .Paint(paint_info);
      continue;
    }

    if (child_container->IsColumnBox()) {
      // This is a fragmentainer, and when node inside a fragmentation context
      // paints multiple block fragments, we need to distinguish between them
      // somehow, for paint caching to work. Therefore, establish a display item
      // scope here.
      unsigned identifier = FragmentainerUniqueIdentifier(
          To<NGPhysicalBoxFragment>(*child_container));
      ScopedDisplayItemFragment scope(paint_info.context, identifier);
      PaintFloatingChildren(*child_container, paint_info, float_paint_info);
    } else {
      PaintFloatingChildren(*child_container, paint_info, float_paint_info);
    }
  }
}

void NGBoxFragmentPainter::PaintFloats(const PaintInfo& paint_info) {
  DCHECK(PhysicalFragment().HasFloatingDescendantsForPaint() ||
         !PhysicalFragment().IsInlineFormattingContext());

  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;
  PaintFloatingChildren(PhysicalFragment(), paint_info, float_paint_info);
}

void NGBoxFragmentPainter::PaintMask(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);
  const NGPhysicalBoxFragment& physical_box_fragment = PhysicalFragment();
  const ComputedStyle& style = physical_box_fragment.Style();
  if (!style.HasMask() || !IsVisibleToPaint(physical_box_fragment, style))
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, GetDisplayItemClient(), paint_info.phase))
    return;

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));

  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           paint_info.phase);
  PhysicalRect paint_rect(paint_offset, box_fragment_.Size());
  const NGBorderEdges& border_edges = BorderEdges();
  PaintMaskImages(paint_info, paint_rect, *box_fragment_.GetLayoutObject(),
                  geometry, border_edges.line_left, border_edges.line_right);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    bool suppress_box_decoration_background) {
  // TODO(mstensho): Break dependency on LayoutObject functionality.
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();

  PhysicalRect paint_rect;
  const DisplayItemClient* background_client = nullptr;
  base::Optional<ScopedBoxContentsPaintState> contents_paint_state;
  bool painting_scrolling_background =
      IsPaintingScrollingBackground(paint_info);
  if (painting_scrolling_background) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    const LayoutBox& layout_box = ToLayoutBox(layout_object);
    paint_rect = layout_box.PhysicalLayoutOverflowRect();

    contents_paint_state.emplace(paint_info, paint_offset, layout_box);
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.Expand(layout_box.BorderBoxOutsets());

    background_client = &layout_box.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
  } else {
    paint_rect.offset = paint_offset;
    paint_rect.size = box_fragment_.Size();
    if (layout_object.IsTableCell()) {
      paint_rect.size =
          PhysicalSize(ToLayoutBox(layout_object).PixelSnappedSize());
    }
    background_client = &GetDisplayItemClient();
  }

  if (!suppress_box_decoration_background) {
    // The fieldset painter is not skipped when there is no background because
    // the legend needs to paint.
    if (PhysicalFragment().IsFieldsetContainer()) {
      NGFieldsetPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_offset);
    } else if (box_fragment_.Style().HasBoxDecorationBackground()) {
      PaintBoxDecorationBackgroundWithRect(
          contents_paint_state ? contents_paint_state->GetPaintInfo()
                               : paint_info,
          paint_rect, *background_client);
    }
  }

  if (ShouldRecordHitTestData(paint_info)) {
    paint_info.context.GetPaintController().RecordHitTestData(
        *background_client, PixelSnappedIntRect(paint_rect),
        PhysicalFragment().EffectiveAllowedTouchAction());
  }

  bool needs_scroll_hit_test = true;
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Pre-CompositeAfterPaint, there is no need to emit scroll hit test
    // display items for composited scrollers because these display items are
    // only used to create non-fast scrollable regions for non-composited
    // scrollers. With CompositeAfterPaint, we always paint the scroll hit
    // test display items but ignore the non-fast region if the scroll was
    // composited in PaintArtifactCompositor::UpdateNonFastScrollableRegions.
    const auto* layer = PhysicalFragment().Layer();
    if (layer && layer->GetCompositedLayerMapping() &&
        layer->GetCompositedLayerMapping()->HasScrollingLayer()) {
      needs_scroll_hit_test = false;
    }
  }

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (!painting_scrolling_background && needs_scroll_hit_test)
    RecordScrollHitTestData(paint_info, *background_client);
}

void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  const LayoutBox& layout_box = ToLayoutBox(*box_fragment_.GetLayoutObject());

  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      ShouldSkipPaintUnderInvalidationChecking(layout_box))
    cache_skipper.emplace(paint_info.context);

  BoxDecorationData box_decoration_data(paint_info, PhysicalFragment());
  if (!box_decoration_data.ShouldPaint())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, background_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, background_client,
                           DisplayItem::kBoxDecorationBackground);

  PaintBoxDecorationBackgroundWithRectImpl(paint_info, paint_rect,
                                           box_decoration_data);
}
// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRectImpl(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = ToLayoutBox(layout_object);

  const ComputedStyle& style = box_fragment_.Style();

  GraphicsContextStateSaver state_saver(paint_info.context, false);

  const NGBorderEdges& border_edges = BorderEdges();
  if (box_decoration_data.ShouldPaintShadow()) {
    PaintNormalBoxShadow(paint_info, paint_rect, style, border_edges.line_left,
                         border_edges.line_right,
                         !box_decoration_data.ShouldPaintBackground());
  }

  bool needs_end_layer = false;
  if (!box_decoration_data.IsPaintingScrollingBackground()) {
    if (box_fragment_.HasSelfPaintingLayer() && layout_box.IsTableCell() &&
        ToInterface<LayoutNGTableCellInterface>(layout_box)
            .TableInterface()
            ->ShouldCollapseBorders()) {
      // We have to clip here because the background would paint on top of the
      // collapsed table borders otherwise, since this is a self-painting layer.
      PhysicalRect clip_rect = paint_rect;
      clip_rect.Expand(layout_box.BorderInsets());
      state_saver.Save();
      paint_info.context.Clip(PixelSnappedIntRect(clip_rect));
    } else if (BleedAvoidanceIsClipping(
                   box_decoration_data.GetBackgroundBleedAvoidance())) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(
          paint_rect.ToLayoutRect(), border_edges.line_left,
          border_edges.line_right);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.GetBackgroundBleedAvoidance() ==
          kBackgroundBleedClipLayer) {
        paint_info.context.BeginLayer();
        needs_end_layer = true;
      }
    }
  }

  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.HasAppearance() &&
      !theme_painter.Paint(layout_box, paint_info, snapped_paint_rect);
  if (!theme_painted) {
    if (box_decoration_data.ShouldPaintBackground()) {
      PaintBackground(paint_info, paint_rect,
                      box_decoration_data.BackgroundColor(),
                      box_decoration_data.GetBackgroundBleedAvoidance());
    }
    if (box_decoration_data.HasAppearance()) {
      theme_painter.PaintDecorations(layout_box.GetNode(),
                                     layout_box.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (box_decoration_data.ShouldPaintShadow()) {
    if (layout_box.IsTableCell()) {
      PhysicalRect inner_rect = paint_rect;
      inner_rect.Contract(layout_box.BorderBoxOutsets());
      // PaintInsetBoxShadowWithInnerRect doesn't subtract borders before
      // painting. We have to use it here after subtracting collapsed borders
      // above. PaintInsetBoxShadowWithBorderRect below subtracts the borders
      // specified on the style object, which doesn't account for border
      // collapsing.
      BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, inner_rect,
                                                       style);
    } else {
      PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect, style,
                                        border_edges.line_left,
                                        border_edges.line_right);
    }
  }

  // The theme will tell us whether or not we should also paint the CSS
  // border.
  if (box_decoration_data.ShouldPaintBorder()) {
    if (!theme_painted) {
      theme_painted =
          box_decoration_data.HasAppearance() &&
          !LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box.GetNode(), style, paint_info, snapped_paint_rect);
    }
    if (!theme_painted) {
      Node* generating_node = layout_object.GeneratingNode();
      const Document& document = layout_object.GetDocument();
      PaintBorder(*box_fragment_.GetLayoutObject(), document, generating_node,
                  paint_info, paint_rect, style,
                  box_decoration_data.GetBackgroundBleedAvoidance(),
                  border_edges.line_left, border_edges.line_right);
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

void NGBoxFragmentPainter::PaintColumnRules(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const ComputedStyle& style = box_fragment_.Style();
  DCHECK(box_fragment_.IsCSSBox());
  DCHECK(style.HasColumnRule());

  // TODO(crbug.com/792437): Certain rule styles should be converted.
  EBorderStyle rule_style = style.ColumnRuleStyle();

  if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context,
                                                  GetDisplayItemClient(),
                                                  DisplayItem::kColumnRules))
    return;

  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           DisplayItem::kColumnRules);

  const Color& rule_color =
      LayoutObject::ResolveColor(style, GetCSSPropertyColumnRuleColor());
  LayoutUnit rule_thickness(style.ColumnRuleWidth());
  PhysicalRect previous_column;
  bool past_first_column_in_row = false;
  for (const NGLink& child : box_fragment_.Children()) {
    if (!child->IsColumnBox()) {
      // Column spanner. Continue in the next row, if there are 2 columns or
      // more there.
      past_first_column_in_row = false;
      previous_column = PhysicalRect();
      continue;
    }

    PhysicalRect current_column(child.offset, child->Size());
    if (!past_first_column_in_row) {
      // Rules are painted *between* columns. Need to see if we have a second
      // one before painting anything.
      past_first_column_in_row = true;
      previous_column = current_column;
      continue;
    }

    PhysicalRect rule;
    BoxSide box_side;
    if (previous_column.Y() == current_column.Y() ||
        previous_column.Bottom() == current_column.Bottom()) {
      // Horizontal writing-mode.
      DCHECK(style.IsHorizontalWritingMode());
      LayoutUnit center;
      if (previous_column.X() < current_column.X()) {
        // Left to right.
        center = (previous_column.X() + current_column.Right()) / 2;
        box_side = BoxSide::kLeft;
      } else {
        // Right to left.
        center = (current_column.X() + previous_column.Right()) / 2;
        box_side = BoxSide::kRight;
      }
      LayoutUnit rule_length = previous_column.Height();
      DCHECK_GE(rule_length, current_column.Height());
      rule.offset.top = previous_column.offset.top;
      rule.size.height = rule_length;
      rule.offset.left = center - rule_thickness / 2;
      rule.size.width = rule_thickness;
    } else {
      // Vertical writing-mode.
      LayoutUnit center;
      if (previous_column.Y() < current_column.Y()) {
        // Top to bottom.
        center = (previous_column.Y() + current_column.Bottom()) / 2;
        box_side = BoxSide::kTop;
      } else {
        // Bottom to top.
        center = (current_column.Y() + previous_column.Bottom()) / 2;
        box_side = BoxSide::kBottom;
      }
      LayoutUnit rule_length = previous_column.Width();
      DCHECK_GE(rule_length, current_column.Width());
      rule.offset.left = previous_column.offset.left;
      rule.size.width = rule_length;
      rule.offset.top = center - rule_thickness / 2;
      rule.size.height = rule_thickness;
    }

    // TODO(crbug.com/792435): The spec actually kind of says that the rules
    // should be as tall as the entire multicol container, not just as tall as
    // the column fragments (this difference matters when block-size is
    // specified and columns are balanced).

    rule.Move(paint_offset);
    IntRect snapped_rule = PixelSnappedIntRect(rule);
    ObjectPainter::DrawLineForBoxSide(paint_info.context, snapped_rule.X(),
                                      snapped_rule.Y(), snapped_rule.MaxX(),
                                      snapped_rule.MaxY(), box_side, rule_color,
                                      rule_style, 0, 0, true);

    previous_column = current_column;
  }
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const Color& background_color,
    BackgroundBleedAvoidance bleed_avoidance) {
  const LayoutBox& layout_box = ToLayoutBox(*box_fragment_.GetLayoutObject());
  if (layout_box.BackgroundTransfersToView())
    return;
  if (layout_box.BackgroundIsKnownToBeObscured())
    return;

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(layout_box);
  PaintFillLayers(paint_info, background_color,
                  box_fragment_.Style().BackgroundLayers(), paint_rect,
                  geometry, bleed_avoidance);
}

void NGBoxFragmentPainter::PaintInlineChildBoxUsingLegacyFallback(
    const NGPhysicalFragment& fragment,
    const PaintInfo& paint_info) {
  const LayoutObject* child_layout_object = fragment.GetLayoutObject();
  DCHECK(child_layout_object);
  if (child_layout_object->PaintFragment()) {
    // This object will use NGBoxFragmentPainter.
    child_layout_object->Paint(paint_info);
    return;
  }

  if (child_layout_object->IsAtomicInlineLevel()) {
    // Pre-NG painters also expect callers to use |PaintAllPhasesAtomically()|
    // for atomic inlines.
    ObjectPainter(*child_layout_object).PaintAllPhasesAtomically(paint_info);
    return;
  }

  child_layout_object->Paint(paint_info);
}

void NGBoxFragmentPainter::PaintAllPhasesAtomically(
    const PaintInfo& paint_info) {
  // Self-painting AtomicInlines should go to normal paint logic.
  DCHECK(!(PhysicalFragment().IsPaintedAtomically() &&
           box_fragment_.HasSelfPaintingLayer()));

  // Pass PaintPhaseSelection and PaintPhaseTextClip is handled by the regular
  // foreground paint implementation. We don't need complete painting for these
  // phases.
  PaintPhase phase = paint_info.phase;
  if (phase == PaintPhase::kSelectionDragImage ||
      phase == PaintPhase::kTextClip)
    return PaintInternal(paint_info);

  if (phase != PaintPhase::kForeground)
    return;

  PaintInfo local_paint_info(paint_info);
  local_paint_info.phase = PaintPhase::kBlockBackground;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kForcedColorsModeBackplate;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kFloat;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kForeground;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kOutline;
  PaintInternal(local_paint_info);
}

void NGBoxFragmentPainter::PaintInlineItems(const PaintInfo& paint_info,
                                            const PhysicalOffset& paint_offset,
                                            const PhysicalOffset& parent_offset,
                                            NGInlineCursor* cursor) {
  while (*cursor) {
    const NGFragmentItem* item = cursor->CurrentItem();
    DCHECK(item);
    switch (item->Type()) {
      case NGFragmentItem::kText:
      case NGFragmentItem::kGeneratedText:
        if (!item->IsHiddenForPaint())
          PaintTextItem(*cursor, paint_info, paint_offset, parent_offset);
        cursor->MoveToNext();
        break;
      case NGFragmentItem::kBox:
        if (!item->IsHiddenForPaint())
          PaintBoxItem(*item, *cursor, paint_info, paint_offset, parent_offset);
        cursor->MoveToNextSkippingChildren();
        break;
      case NGFragmentItem::kLine:
        NOTREACHED();
        cursor->MoveToNext();
        break;
    }
  }
}

// Paint a line box. This function paints hit tests and backgrounds of
// `::first-line`. In all other cases, the container box paints background.
inline void NGBoxFragmentPainter::PaintLineBox(
    const NGPhysicalFragment& line_box_fragment,
    const DisplayItemClient& display_item_client,
    const NGPaintFragment* line_box_paint_fragment,
    const NGFragmentItem* line_box_item,
    const PaintInfo& paint_info,
    const PhysicalOffset& child_offset) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  if (ShouldRecordHitTestData(paint_info)) {
    PhysicalRect border_box = line_box_fragment.LocalRect();
    border_box.offset += child_offset;
    paint_info.context.GetPaintController().RecordHitTestData(
        display_item_client, PixelSnappedIntRect(border_box),
        PhysicalFragment().EffectiveAllowedTouchAction());
  }
  if (NGLineBoxFragmentPainter::NeedsPaint(line_box_fragment)) {
    NGLineBoxFragmentPainter line_box_painter(
        line_box_fragment, line_box_paint_fragment, line_box_item,
        PhysicalFragment(), paint_fragment_);
    line_box_painter.PaintBackgroundBorderShadow(paint_info, child_offset);
  }
}

void NGBoxFragmentPainter::PaintLineBoxChildren(
    NGInlineCursor* children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask &&
      paint_info.phase != PaintPhase::kDescendantOutlinesOnly &&
      paint_info.phase != PaintPhase::kOutline)
    return;

  // The only way an inline could paint like this is if it has a layer.
  const auto* layout_object = box_fragment_.GetLayoutObject();
  DCHECK(!layout_object || layout_object->IsLayoutBlock() ||
         (layout_object->IsLayoutInline() && layout_object->HasLayer()));

  if (paint_info.phase == PaintPhase::kForeground &&
      paint_info.ShouldAddUrlMetadata()) {
    AddURLRectsForInlineChildrenRecursively(*layout_object, paint_info,
                                            paint_offset);
  }

  // If we have no lines then we have no work to do.
  if (!*children)
    return;

  if (paint_info.phase == PaintPhase::kForcedColorsModeBackplate &&
      box_fragment_.GetDocument().InForcedColorsMode()) {
    PaintBackplate(children, paint_info, paint_offset);
    return;
  }

  if (UNLIKELY(children->IsItemCursor())) {
    PaintLineBoxChildItems(children, paint_info, paint_offset);
    return;
  }

  const bool is_horizontal = box_fragment_.Style().IsHorizontalWritingMode();
  for (; *children; children->MoveToNextSkippingChildren()) {
    const NGPaintFragment* line = children->CurrentPaintFragment();
    DCHECK(line);
    const NGPhysicalFragment& child_fragment = line->PhysicalFragment();
    DCHECK(!child_fragment.IsOutOfFlowPositioned());
    if (child_fragment.IsFloating())
      continue;

    // Check if CullRect intersects with this child, only in block direction
    // because soft-wrap and <br> needs to paint outside of InkOverflow() in
    // inline direction.
    const PhysicalOffset child_offset = paint_offset + line->Offset();
    PhysicalRect child_rect = line->InkOverflow();
    if (is_horizontal) {
      LayoutUnit y = child_rect.offset.top + child_offset.top;
      if (!paint_info.GetCullRect().IntersectsVerticalRange(
              y, y + child_rect.size.height))
        continue;
    } else {
      LayoutUnit x = child_rect.offset.left + child_offset.left;
      if (!paint_info.GetCullRect().IntersectsHorizontalRange(
              x, x + child_rect.size.width))
        continue;
    }

    if (child_fragment.IsListMarker()) {
      PaintAtomicInlineChild(*line, paint_info);
      continue;
    }
    DCHECK(child_fragment.IsLineBox());
    PaintLineBox(child_fragment, *line, line, /* line_box_item */ nullptr,
                 paint_info, child_offset);
    PaintInlineChildren(line->Children(), paint_info, child_offset);
  }
}

void NGBoxFragmentPainter::PaintLineBoxChildItems(
    NGInlineCursor* children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const bool is_horizontal = box_fragment_.Style().IsHorizontalWritingMode();
  for (; *children; children->MoveToNextSkippingChildren()) {
    const NGFragmentItem* child_item = children->CurrentItem();
    DCHECK(child_item);

    // Check if CullRect intersects with this child, only in block direction
    // because soft-wrap and <br> needs to paint outside of InkOverflow() in
    // inline direction.
    const PhysicalOffset& child_offset =
        paint_offset + child_item->OffsetInContainerBlock();
    const PhysicalRect child_rect = child_item->InkOverflow();
    if (is_horizontal) {
      LayoutUnit y = child_rect.offset.top + child_offset.top;
      if (!paint_info.GetCullRect().IntersectsVerticalRange(
              y, y + child_rect.size.height))
        continue;
    } else {
      LayoutUnit x = child_rect.offset.left + child_offset.left;
      if (!paint_info.GetCullRect().IntersectsHorizontalRange(
              x, x + child_rect.size.width))
        continue;
    }

    if (child_item->Type() == NGFragmentItem::kLine) {
      const NGPhysicalLineBoxFragment* line_box_fragment =
          child_item->LineBoxFragment();
      DCHECK(line_box_fragment);
      PaintLineBox(*line_box_fragment, *child_item,
                   /* line_box_paint_fragment */ nullptr, child_item,
                   paint_info, child_offset);
      NGInlineCursor line_box_cursor = children->CursorForDescendants();
      PaintInlineItems(paint_info, paint_offset,
                       child_item->OffsetInContainerBlock(), &line_box_cursor);
      continue;
    }

    if (const NGPhysicalBoxFragment* child_fragment =
            child_item->BoxFragment()) {
      if (child_fragment->IsListMarker()) {
        PaintBoxItem(*child_item, *child_fragment, *children, paint_info,
                     paint_offset);
        continue;
      }
    }

    NOTREACHED();
  }
}

void NGBoxFragmentPainter::PaintBackplate(NGInlineCursor* line_boxes,
                                          const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset) {
  if (paint_info.phase != PaintPhase::kForcedColorsModeBackplate)
    return;

  // Only paint backplates behind text when forced-color-adjust is auto.
  const ComputedStyle& style = PhysicalFragment().Style();
  if (style.ForcedColorAdjust() == EForcedColorAdjust::kNone)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, GetDisplayItemClient(),
          DisplayItem::kForcedColorsModeBackplate))
    return;

  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           DisplayItem::kForcedColorsModeBackplate);
  Color backplate_color = style.ForcedBackplateColor();
  const auto& backplates = BuildBackplate(line_boxes, paint_offset);
  for (const auto backplate : backplates)
    paint_info.context.FillRect(FloatRect(backplate), backplate_color);
}

void NGBoxFragmentPainter::PaintInlineChildren(
    NGPaintFragment::ChildList inline_children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  // TODO(kojii): Move kOutline painting into a |PaintInlineChildrenOutlines()|
  // method instead as it would be more efficient. Would require repeating some
  // of the code below though.
  // This DCHECK can then match to |InlineFlowBoxPainter::Paint|.
  DCHECK_NE(paint_info.phase, PaintPhase::kDescendantOutlinesOnly);

  for (const NGPaintFragment* child : inline_children) {
    const NGPhysicalFragment& child_fragment = child->PhysicalFragment();
    if (child_fragment.IsHiddenForPaint())
      continue;
    if (child_fragment.IsFloating())
      continue;

    // Skip if this child does not intersect with CullRect.
    if (!paint_info.IntersectsCullRect(child->InkOverflow(),
                                       paint_offset + child->Offset()) &&
        // Don't skip empty size text in order to paint selection for <br>.
        !(child_fragment.IsText() && child_fragment.Size().IsEmpty() &&
          HasSelection(child_fragment.GetLayoutObject())))
      continue;

    if (child_fragment.Type() == NGPhysicalFragment::kFragmentText) {
      DCHECK(!child_fragment.HasSelfPaintingLayer() ||
             To<NGPhysicalTextFragment>(child_fragment).IsEllipsis());
      PaintTextChild(*child, paint_info, paint_offset);
    } else if (child_fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (child_fragment.HasSelfPaintingLayer())
        continue;
      if (child_fragment.IsAtomicInline())
        PaintAtomicInlineChild(*child, paint_info);
      else
        NGInlineBoxFragmentPainter(*child).Paint(paint_info, paint_offset);
    } else {
      NOTREACHED();
    }
  }
}

void NGBoxFragmentPainter::PaintAtomicInlineChild(const NGPaintFragment& child,
                                                  const PaintInfo& paint_info) {
  // Inline children should be painted by PaintInlineChild.
  DCHECK(child.PhysicalFragment().IsAtomicInline());

  const NGPhysicalFragment& fragment = child.PhysicalFragment();
  if (child.HasSelfPaintingLayer())
    return;
  if (fragment.Type() == NGPhysicalFragment::kFragmentBox &&
      FragmentRequiresLegacyFallback(fragment)) {
    PaintInlineChildBoxUsingLegacyFallback(fragment, paint_info);
  } else {
    NGBoxFragmentPainter(child).PaintAllPhasesAtomically(paint_info);
  }
}

void NGBoxFragmentPainter::PaintTextChild(const NGPaintFragment& paint_fragment,
                                          const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset) {
  // Inline blocks should be painted by PaintAtomicInlineChild.
  DCHECK(!paint_fragment.PhysicalFragment().IsAtomicInline());

  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  NGTextPainterCursor cursor(paint_fragment);
  NGTextFragmentPainter<NGTextPainterCursor> text_painter(cursor);
  text_painter.Paint(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintTextItem(const NGInlineCursor& cursor,
                                         const PaintInfo& paint_info,
                                         const PhysicalOffset& paint_offset,
                                         const PhysicalOffset& parent_offset) {
  DCHECK(cursor.CurrentItem());
  const NGFragmentItem& item = *cursor.CurrentItem();
  DCHECK(item.IsText()) << item;

  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          item.InkOverflow(), paint_offset + item.OffsetInContainerBlock()) &&
      // Don't skip <br>, it doesn't have ink but need to paint selection.
      !(item.IsLineBreak() && HasSelection(item.GetLayoutObject())))
    return;

  NGTextFragmentPainter<NGInlineCursor> text_painter(cursor, parent_offset);
  text_painter.Paint(paint_info, paint_offset);
}

// Paint non-culled box item.
void NGBoxFragmentPainter::PaintBoxItem(
    const NGFragmentItem& item,
    const NGPhysicalBoxFragment& child_fragment,
    const NGInlineCursor& cursor,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK_EQ(item.Type(), NGFragmentItem::kBox);
  DCHECK_EQ(&item, cursor.Current().Item());
  DCHECK_EQ(item.BoxFragment(), &child_fragment);
  DCHECK(!child_fragment.IsHiddenForPaint());
  if (child_fragment.HasSelfPaintingLayer() || child_fragment.IsFloating())
    return;

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          item.InkOverflow(), paint_offset + item.OffsetInContainerBlock()))
    return;

  if (child_fragment.IsAtomicInline() || child_fragment.IsListMarker()) {
    if (FragmentRequiresLegacyFallback(child_fragment)) {
      PaintInlineChildBoxUsingLegacyFallback(child_fragment, paint_info);
      return;
    }
    NGBoxFragmentPainter(child_fragment).PaintAllPhasesAtomically(paint_info);
    return;
  }

  DCHECK(child_fragment.IsInlineBox());
  NGInlineBoxFragmentPainter(cursor, item, child_fragment)
      .Paint(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintBoxItem(const NGFragmentItem& item,
                                        const NGInlineCursor& cursor,
                                        const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset,
                                        const PhysicalOffset& parent_offset) {
  DCHECK_EQ(item.Type(), NGFragmentItem::kBox);
  DCHECK_EQ(&item, cursor.Current().Item());

  if (const NGPhysicalBoxFragment* child_fragment = item.BoxFragment()) {
    PaintBoxItem(item, *child_fragment, cursor, paint_info, paint_offset);
    return;
  }

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          item.InkOverflow(), paint_offset + item.OffsetInContainerBlock()))
    return;

  // This |item| is a culled inline box.
  DCHECK(item.GetLayoutObject()->IsLayoutInline());
  NGInlineCursor children = cursor.CursorForDescendants();
  // Pass the given |parent_offset| because culled inline boxes do not affect
  // the sub-pixel snapping behavior. TODO(kojii): This is for the
  // compatibility, we may want to revisit in future.
  PaintInlineItems(paint_info, paint_offset, parent_offset, &children);
}

bool NGBoxFragmentPainter::IsPaintingScrollingBackground(
    const PaintInfo& paint_info) const {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return paint_info.IsPaintingScrollingBackground();

  // TODO(layout-dev): Change paint_info.PaintContainer to accept fragments
  // once LayoutNG supports scrolling containers.
  return paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents &&
         !(paint_info.PaintFlags() &
           kPaintLayerPaintingCompositingBackgroundPhase) &&
         box_fragment_.GetLayoutObject() == paint_info.PaintContainer();
}

bool NGBoxFragmentPainter::ShouldPaint(
    const ScopedPaintState& paint_state) const {
  // TODO(layout-dev): Add support for scrolling, see BlockPainter::ShouldPaint.
  if (paint_fragment_) {
    return paint_state.LocalRectIntersectsCullRect(
        paint_fragment_->InkOverflow());
  }
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  if (!fragment.IsInlineBox()) {
    return paint_state.LocalRectIntersectsCullRect(
        ToLayoutBox(fragment.GetLayoutObject())->PhysicalVisualOverflowRect());
  }
  NOTREACHED();
  return false;
}

void NGBoxFragmentPainter::PaintTextClipMask(GraphicsContext& context,
                                             const IntRect& mask_rect,
                                             const PhysicalOffset& paint_offset,
                                             bool object_has_multiple_boxes) {
  PaintInfo paint_info(context, mask_rect, PaintPhase::kTextClip,
                       kGlobalPaintNormalPhase, 0);
  if (!object_has_multiple_boxes) {
    PaintObject(paint_info, paint_offset);
    return;
  }

  if (paint_fragment_) {
    NGInlineBoxFragmentPainter inline_box_painter(*paint_fragment_);
    PaintTextClipMask(paint_info, paint_offset - paint_fragment_->Offset(),
                      &inline_box_painter);
    return;
  }

  DCHECK(inline_box_cursor_);
  DCHECK(box_item_);
  NGInlineBoxFragmentPainter inline_box_painter(*inline_box_cursor_,
                                                *box_item_);
  PaintTextClipMask(paint_info,
                    paint_offset - box_item_->OffsetInContainerBlock(),
                    &inline_box_painter);
}

void NGBoxFragmentPainter::PaintTextClipMask(
    const PaintInfo& paint_info,
    PhysicalOffset paint_offset,
    NGInlineBoxFragmentPainter* inline_box_painter) {
  const ComputedStyle& style = box_fragment_.Style();
  if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice) {
    LayoutUnit offset_on_line;
    LayoutUnit total_width;
    inline_box_painter->ComputeFragmentOffsetOnLine(
        style.Direction(), &offset_on_line, &total_width);
    if (style.IsHorizontalWritingMode())
      paint_offset.left += offset_on_line;
    else
      paint_offset.top += offset_on_line;
  }
  inline_box_painter->Paint(paint_info, paint_offset);
}

PhysicalRect NGBoxFragmentPainter::AdjustRectForScrolledContent(
    const PaintInfo& paint_info,
    const BoxPainterBase::FillLayerInfo& info,
    const PhysicalRect& rect) {
  PhysicalRect scrolled_paint_rect = rect;
  GraphicsContext& context = paint_info.context;
  const NGPhysicalBoxFragment& physical = PhysicalFragment();

  // Clip to the overflow area.
  if (info.is_clipped_with_local_scrolling &&
      !IsPaintingScrollingBackground(paint_info)) {
    context.Clip(FloatRect(physical.OverflowClipRect(rect.offset)));

    // Adjust the paint rect to reflect a scrolled content box with borders at
    // the ends.
    PhysicalOffset offset(physical.PixelSnappedScrolledContentOffset());
    scrolled_paint_rect.Move(-offset);
    LayoutRectOutsets borders = AdjustedBorderOutsets(info);
    scrolled_paint_rect.size =
        physical.ScrollSize() + PhysicalSize(borders.Size());
  }
  return scrolled_paint_rect;
}

LayoutRectOutsets NGBoxFragmentPainter::ComputeBorders() const {
  if (box_fragment_.GetLayoutObject()->IsTableCell())
    return ToLayoutBox(box_fragment_.GetLayoutObject())->BorderBoxOutsets();
  return BoxStrutToLayoutRectOutsets(PhysicalFragment().BorderWidths());
}

LayoutRectOutsets NGBoxFragmentPainter::ComputePadding() const {
  return BoxStrutToLayoutRectOutsets(PhysicalFragment().PixelSnappedPadding());
}

BoxPainterBase::FillLayerInfo NGBoxFragmentPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_scrolling_background) const {
  const NGBorderEdges& border_edges = BorderEdges();
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  return BoxPainterBase::FillLayerInfo(
      fragment.GetLayoutObject()->GetDocument(), fragment.Style(),
      fragment.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      LayoutObject::ShouldRespectImageOrientation(fragment.GetLayoutObject()),
      border_edges.line_left, border_edges.line_right,
      fragment.GetLayoutObject()->IsLayoutInline(),
      is_painting_scrolling_background);
}

bool NGBoxFragmentPainter::HitTestContext::AddNodeToResult(
    Node* node,
    const PhysicalRect& bounds_rect,
    const PhysicalOffset& offset) const {
  if (node && !result->InnerNode())
    result->SetNodeAndPosition(node, location.Point() - offset);
  return result->AddNodeToListBasedTestResult(node, location, bounds_rect) ==
         kStopHitTesting;
}

bool NGBoxFragmentPainter::NodeAtPoint(HitTestResult& result,
                                       const HitTestLocation& hit_test_location,
                                       const PhysicalOffset& physical_offset,
                                       HitTestAction action) {
  HitTestContext hit_test(action, hit_test_location, physical_offset, &result);
  return NodeAtPoint(hit_test, physical_offset);
}

bool NGBoxFragmentPainter::NodeAtPoint(HitTestResult& result,
                                       const HitTestLocation& hit_test_location,
                                       const PhysicalOffset& physical_offset,
                                       const PhysicalOffset& inline_root_offset,
                                       HitTestAction action) {
  HitTestContext hit_test(action, hit_test_location, inline_root_offset,
                          &result);
  return NodeAtPoint(hit_test, physical_offset);
}

bool NGBoxFragmentPainter::NodeAtPoint(const HitTestContext& hit_test,
                                       const PhysicalOffset& physical_offset) {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  const PhysicalSize& size = box_fragment_.Size();
  const ComputedStyle& style = box_fragment_.Style();

  bool hit_test_self = fragment.IsInSelfHitTestingPhase(hit_test.action);

  if (hit_test_self && box_fragment_.HasOverflowClip() &&
      HitTestOverflowControl(hit_test, physical_offset))
    return true;

  const LayoutObject* layout_object = PhysicalFragment().GetLayoutObject();
  bool skip_children =
      layout_object &&
      layout_object == hit_test.result->GetHitTestRequest().GetStopNode();
  if (!skip_children && box_fragment_.ShouldClipOverflow()) {
    // PaintLayer::HitTestContentsForFragments checked the fragments'
    // foreground rect for intersection if a layer is self painting,
    // so only do the overflow clip check here for non-self-painting layers.
    if (!box_fragment_.HasSelfPaintingLayer() &&
        !hit_test.location.Intersects(PhysicalFragment().OverflowClipRect(
            physical_offset, kExcludeOverlayScrollbarSizeForHitTesting))) {
      skip_children = true;
    }
    if (!skip_children && style.HasBorderRadius()) {
      PhysicalRect bounds_rect(physical_offset, size);
      skip_children = !hit_test.location.Intersects(
          style.GetRoundedInnerBorderFor(bounds_rect.ToLayoutRect()));
    }
  }

  if (!skip_children) {
    if (!box_fragment_.HasOverflowClip()) {
      if (HitTestChildren(hit_test, physical_offset))
        return true;
    } else {
      const PhysicalOffset scrolled_offset =
          physical_offset -
          PhysicalOffset(
              PhysicalFragment().PixelSnappedScrolledContentOffset());
      HitTestContext adjusted_hit_test(hit_test.action, hit_test.location,
                                       scrolled_offset, hit_test.result);
      if (HitTestChildren(adjusted_hit_test, scrolled_offset))
        return true;
    }
  }

  if (style.HasBorderRadius() &&
      HitTestClippedOutByBorder(hit_test.location, physical_offset))
    return false;

  // Now hit test ourselves.
  if (hit_test_self &&
      VisibleToHitTestRequest(hit_test.result->GetHitTestRequest())) {
    PhysicalRect bounds_rect(physical_offset, size);
    if (UNLIKELY(hit_test.result->GetHitTestRequest().GetType() &
                 HitTestRequest::kHitTestVisualOverflow)) {
      bounds_rect = SelfInkOverflow();
      bounds_rect.Move(physical_offset);
    }
    // TODO(kojii): Don't have good explanation why only inline box needs to
    // snap, but matches to legacy and fixes crbug.com/976606.
    if (fragment.IsInlineBox())
      bounds_rect = PhysicalRect(PixelSnappedIntRect(bounds_rect));
    if (hit_test.location.Intersects(bounds_rect)) {
      // We set offset in container block instead of offset in |fragment| like
      // |NGBoxFragmentPainter::HitTestTextFragment()|.
      // See http://crbug.com/1043471
      if (box_item_ && box_item_->IsInlineBox()) {
        if (hit_test.AddNodeToResult(
                fragment.NodeForHitTest(), bounds_rect,
                physical_offset - box_item_->OffsetInContainerBlock()))
          return true;
      } else if (paint_fragment_ &&
                 paint_fragment_->PhysicalFragment().IsInline()) {
        if (hit_test.AddNodeToResult(
                fragment.NodeForHitTest(), bounds_rect,
                physical_offset - paint_fragment_->OffsetInContainerBlock()))
          return true;
      } else {
        if (hit_test.AddNodeToResult(fragment.NodeForHitTest(), bounds_rect,
                                     physical_offset))
          return true;
      }
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestAllPhases(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestFilter hit_test_filter) {
  // Logic taken from LayoutObject::HitTestAllPhases().
  HitTestContext hit_test(kHitTestForeground, hit_test_location,
                          accumulated_offset, &result);
  bool inside = false;
  if (hit_test_filter != kHitTestSelf) {
    // First test the foreground layer (lines and inlines).
    inside = NodeAtPoint(hit_test, accumulated_offset);

    // Test floats next.
    if (!inside) {
      hit_test.action = kHitTestFloat;
      inside = NodeAtPoint(hit_test, accumulated_offset);
    }

    // Finally test to see if the mouse is in the background (within a child
    // block's background).
    if (!inside) {
      hit_test.action = kHitTestChildBlockBackgrounds;
      inside = NodeAtPoint(hit_test, accumulated_offset);
    }
  }

  // See if the pointer is inside us but not any of our descendants.
  if (hit_test_filter != kHitTestDescendants && !inside) {
    hit_test.action = kHitTestChildBlockBackground;
    inside = NodeAtPoint(hit_test, accumulated_offset);
  }

  return inside;
}

bool NGBoxFragmentPainter::VisibleToHitTestRequest(
    const HitTestRequest& request) const {
  return FragmentVisibleToHitTestRequest(box_fragment_, request);
}

bool NGBoxFragmentPainter::HitTestTextFragment(
    const HitTestContext& hit_test,
    const NGInlineBackwardCursor& cursor,
    const PhysicalOffset& physical_offset) {
  if (hit_test.action != kHitTestForeground)
    return false;

  const NGPaintFragment* text_paint_fragment = cursor.Current().PaintFragment();
  DCHECK(text_paint_fragment);
  const auto& text_fragment =
      To<NGPhysicalTextFragment>(text_paint_fragment->PhysicalFragment());
  if (!FragmentVisibleToHitTestRequest(text_fragment,
                                       hit_test.result->GetHitTestRequest()))
    return false;

  // TODO(layout-dev): Clip to line-top/bottom.
  PhysicalRect border_rect(physical_offset, text_fragment.Size());
  PhysicalRect rect(PixelSnappedIntRect(border_rect));
  if (UNLIKELY(hit_test.result->GetHitTestRequest().GetType() &
               HitTestRequest::kHitTestVisualOverflow)) {
    rect = text_fragment.SelfInkOverflow();
    rect.Move(border_rect.offset);
  }
  if (!hit_test.location.Intersects(rect))
    return false;

  return hit_test.AddNodeToResult(
      text_fragment.NodeForHitTest(), rect,
      physical_offset - text_paint_fragment->OffsetInContainerBlock());
}

bool NGBoxFragmentPainter::HitTestTextItem(const HitTestContext& hit_test,
                                           const NGFragmentItem& text_item) {
  DCHECK(text_item.IsText());

  if (hit_test.action != kHitTestForeground)
    return false;
  if (!IsVisibleToHitTest(text_item, hit_test.result->GetHitTestRequest()))
    return false;

  // TODO(layout-dev): Clip to line-top/bottom.
  const PhysicalOffset offset =
      hit_test.inline_root_offset + text_item.OffsetInContainerBlock();
  PhysicalRect border_rect(offset, text_item.Size());
  PhysicalRect rect(PixelSnappedIntRect(border_rect));
  if (UNLIKELY(hit_test.result->GetHitTestRequest().GetType() &
               HitTestRequest::kHitTestVisualOverflow)) {
    rect = text_item.SelfInkOverflow();
    rect.Move(border_rect.offset);
  }
  if (!hit_test.location.Intersects(rect))
    return false;

  return hit_test.AddNodeToResult(text_item.NodeForHitTest(), rect,
                                  hit_test.inline_root_offset);
}

// Replicates logic in legacy InlineFlowBox::NodeAtPoint().
bool NGBoxFragmentPainter::HitTestLineBoxFragment(
    const HitTestContext& hit_test,
    const NGPhysicalLineBoxFragment& fragment,
    const NGInlineBackwardCursor& cursor,
    const PhysicalOffset& physical_offset) {
  if (HitTestChildren(hit_test, PhysicalFragment(),
                      cursor.CursorForDescendants(), physical_offset))
    return true;

  if (hit_test.action != kHitTestForeground)
    return false;

  if (!VisibleToHitTestRequest(hit_test.result->GetHitTestRequest()))
    return false;

  const PhysicalOffset overflow_location =
      cursor.Current().SelfInkOverflow().offset + physical_offset;
  if (HitTestClippedOutByBorder(hit_test.location, overflow_location))
    return false;

  const PhysicalRect bounds_rect(physical_offset, fragment.Size());
  const ComputedStyle& containing_box_style = box_fragment_.Style();
  if (containing_box_style.HasBorderRadius() &&
      !hit_test.location.Intersects(
          containing_box_style.GetRoundedBorderFor(bounds_rect.ToLayoutRect())))
    return false;

  // Now hit test ourselves.
  if (!hit_test.location.Intersects(bounds_rect))
    return false;

  // Floats will be hit-tested in |kHitTestFloat| phase, but
  // |LayoutObject::HitTestAllPhases| does not try it if |kHitTestForeground|
  // succeeds. Pretend the location is not in this linebox if it hits floating
  // descendants. TODO(kojii): Computing this is redundant, consider
  // restructuring. Changing the caller logic isn't easy because currently
  // floats are in the bounds of line boxes only in NG.
  if (fragment.HasFloatingDescendantsForPaint()) {
    DCHECK_NE(hit_test.action, kHitTestFloat);
    HitTestContext hit_test_float = hit_test;
    hit_test_float.action = kHitTestFloat;
    if (HitTestChildren(hit_test_float, PhysicalFragment(),
                        cursor.CursorForDescendants(), physical_offset))
      return false;
  }

  return hit_test.AddNodeToResult(
      fragment.NodeForHitTest(), bounds_rect,
      physical_offset - cursor.Current().OffsetInContainerBlock());
}

bool NGBoxFragmentPainter::HitTestChildBoxFragment(
    const HitTestContext& hit_test,
    const NGPhysicalBoxFragment& fragment,
    const NGInlineBackwardCursor& backward_cursor,
    const PhysicalOffset& physical_offset) {
  // Note: Floats should only be hit tested in the |kHitTestFloat| phase, so we
  // shouldn't enter a float when |action| doesn't match. However, as floats may
  // scatter around in the entire inline formatting context, we should always
  // enter non-floating inline child boxes to search for floats in the
  // |kHitTestFloat| phase, unless the child box forms another context.
  if (fragment.IsFloating() && hit_test.action != kHitTestFloat)
    return false;

  if (!FragmentRequiresLegacyFallback(fragment)) {
    DCHECK(!fragment.IsAtomicInline());
    DCHECK(!fragment.IsFloating());
    if (const NGPaintFragment* paint_fragment =
            backward_cursor.Current().PaintFragment()) {
      if (fragment.IsInlineBox()) {
        return NGBoxFragmentPainter(*paint_fragment)
            .NodeAtPoint(hit_test, physical_offset);
      }
      // When traversing into a different inline formatting context,
      // |inline_root_offset| needs to be updated.
      return NGBoxFragmentPainter(*paint_fragment)
          .NodeAtPoint(*hit_test.result, hit_test.location, physical_offset,
                       hit_test.action);
    }
    NGInlineCursor cursor(backward_cursor);
    const NGFragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    DCHECK_EQ(item->BoxFragment(), &fragment);
    if (fragment.IsInlineBox()) {
      return NGBoxFragmentPainter(cursor, *item, fragment)
          .NodeAtPoint(hit_test, physical_offset);
    }
    // When traversing into a different inline formatting context,
    // |inline_root_offset| needs to be updated.
    return NGBoxFragmentPainter(cursor, *item, fragment)
        .NodeAtPoint(*hit_test.result, hit_test.location, physical_offset,
                     hit_test.action);
  }

  if (fragment.IsInline() && hit_test.action != kHitTestForeground)
    return false;

  if (fragment.IsPaintedAtomically()) {
    return HitTestAllPhasesInFragment(fragment, hit_test.location,
                                      physical_offset, hit_test.result);
  }

  return fragment.GetMutableLayoutObject()->NodeAtPoint(
      *hit_test.result, hit_test.location, physical_offset, hit_test.action);
}

bool NGBoxFragmentPainter::HitTestChildBoxItem(
    const HitTestContext& hit_test,
    const NGPhysicalBoxFragment& container,
    const NGFragmentItem& item,
    const NGInlineBackwardCursor& cursor) {
  DCHECK_EQ(&item, cursor.Current().Item());

  if (const NGPhysicalBoxFragment* child_fragment = item.BoxFragment()) {
    const PhysicalOffset child_offset =
        hit_test.inline_root_offset + item.OffsetInContainerBlock();
    return HitTestChildBoxFragment(hit_test, *child_fragment, cursor,
                                   child_offset);
  }

  DCHECK(item.GetLayoutObject()->IsLayoutInline());
  DCHECK(!ToLayoutInline(item.GetLayoutObject())->ShouldCreateBoxFragment());
  if (NGInlineCursor descendants = cursor.CursorForDescendants()) {
    if (HitTestItemsChildren(hit_test, container, descendants))
      return true;
  }

  // Now hit test ourselves.
  if (hit_test.action == kHitTestForeground &&
      IsVisibleToHitTest(item, hit_test.result->GetHitTestRequest())) {
    const PhysicalOffset child_offset =
        hit_test.inline_root_offset + item.OffsetInContainerBlock();
    PhysicalRect bounds_rect(child_offset, item.Size());
    if (UNLIKELY(hit_test.result->GetHitTestRequest().GetType() &
                 HitTestRequest::kHitTestVisualOverflow)) {
      bounds_rect = item.SelfInkOverflow();
      bounds_rect.Move(child_offset);
    }
    // TODO(kojii): Don't have good explanation why only inline box needs to
    // snap, but matches to legacy and fixes crbug.com/976606.
    bounds_rect = PhysicalRect(PixelSnappedIntRect(bounds_rect));
    if (hit_test.location.Intersects(bounds_rect)) {
      if (hit_test.AddNodeToResult(item.NodeForHitTest(), bounds_rect,
                                   child_offset))
        return true;
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestChildren(
    const HitTestContext& hit_test,
    const PhysicalOffset& accumulated_offset) {
  if (paint_fragment_) {
    NGInlineCursor cursor(*paint_fragment_);
    return HitTestChildren(hit_test, PhysicalFragment(), cursor,
                           accumulated_offset);
  }
  if (UNLIKELY(inline_box_cursor_)) {
    NGInlineCursor descendants = inline_box_cursor_->CursorForDescendants();
    if (descendants) {
      return HitTestChildren(hit_test, PhysicalFragment(), descendants,
                             accumulated_offset);
    }
    return false;
  }
  if (items_) {
    NGInlineCursor cursor(*items_);
    return HitTestChildren(hit_test, PhysicalFragment(), cursor,
                           accumulated_offset);
  }
  // Check descendants of this fragment because floats may be in the
  // |NGFragmentItems| of the descendants.
  if (hit_test.action == kHitTestFloat &&
      box_fragment_.HasFloatingDescendantsForPaint() &&
      RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    return HitTestFloatingChildren(hit_test, box_fragment_, accumulated_offset);
  }

  if (hit_test.action == kHitTestFloat) {
    return box_fragment_.HasFloatingDescendantsForPaint() &&
           HitTestFloatingChildren(hit_test, box_fragment_, accumulated_offset);
  }
  return HitTestBlockChildren(*hit_test.result, hit_test.location,
                              accumulated_offset, hit_test.action);
}

bool NGBoxFragmentPainter::HitTestChildren(
    const HitTestContext& hit_test,
    const NGPhysicalBoxFragment& container,
    const NGInlineCursor& children,
    const PhysicalOffset& accumulated_offset) {
  if (children.IsPaintFragmentCursor())
    return HitTestPaintFragmentChildren(hit_test, children, accumulated_offset);
  if (children.IsItemCursor())
    return HitTestItemsChildren(hit_test, container, children);
  // Hits nothing if there were no children.
  return false;
}

bool NGBoxFragmentPainter::HitTestBlockChildren(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    PhysicalOffset accumulated_offset,
    HitTestAction action) {
  if (action == kHitTestChildBlockBackgrounds)
    action = kHitTestChildBlockBackground;
  auto children = box_fragment_.Children();
  for (const NGLink& child : base::Reversed(children)) {
    const auto& block_child = To<NGPhysicalBoxFragment>(*child);
    if (block_child.HasSelfPaintingLayer() || block_child.IsFloating())
      continue;

    const PhysicalOffset child_offset = accumulated_offset + child.offset;

    bool hit_child =
        block_child.IsPaintedAtomically()
            ? HitTestAllPhasesInFragment(block_child, hit_test_location,
                                         child_offset, &result)
            : NodeAtPointInFragment(block_child, hit_test_location,
                                    child_offset, action, &result);

    if (hit_child) {
      if (const LayoutObject* child_object = block_child.GetLayoutObject()) {
        child_object->UpdateHitTestResult(
            result, hit_test_location.Point() - accumulated_offset);
      }

      // Our child may have been an anonymous-block, update the hit-test node
      // to include our node if needed.
      if (const LayoutObject* object = box_fragment_.GetLayoutObject()) {
        object->UpdateHitTestResult(
            result, hit_test_location.Point() - accumulated_offset);
      }

      return true;
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestPaintFragmentChildren(
    const HitTestContext& hit_test,
    const NGInlineCursor& children,
    const PhysicalOffset& accumulated_offset) {
  DCHECK(children.IsPaintFragmentCursor());
  for (NGInlineBackwardCursor cursor(children); cursor;) {
    const NGPaintFragment* child_paint_fragment =
        cursor.Current().PaintFragment();
    DCHECK(child_paint_fragment);
    const NGPhysicalFragment& child_fragment =
        child_paint_fragment->PhysicalFragment();
    if (child_fragment.HasSelfPaintingLayer()) {
      cursor.MoveToPreviousSibling();
      continue;
    }

    const PhysicalOffset child_offset =
        child_paint_fragment->Offset() + accumulated_offset;
    if (child_fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (HitTestChildBoxFragment(hit_test,
                                  To<NGPhysicalBoxFragment>(child_fragment),
                                  cursor, child_offset))
        return true;
    } else if (child_fragment.Type() == NGPhysicalFragment::kFragmentLineBox) {
      if (HitTestLineBoxFragment(hit_test,
                                 To<NGPhysicalLineBoxFragment>(child_fragment),
                                 cursor, child_offset))
        return true;
    } else if (child_fragment.Type() == NGPhysicalFragment::kFragmentText) {
      if (HitTestTextFragment(hit_test, cursor, child_offset))
        return true;
    }

    cursor.MoveToPreviousSibling();

    if (child_fragment.IsInline() && hit_test.action == kHitTestForeground) {
      // Hit test culled inline boxes between |fragment| and its parent
      // fragment.
      if (HitTestCulledInlineAncestors(*hit_test.result, children,
                                       *child_paint_fragment, cursor.Current(),
                                       hit_test.location, child_offset))
        return true;
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestItemsChildren(
    const HitTestContext& hit_test,
    const NGPhysicalBoxFragment& container,
    const NGInlineCursor& children) {
  DCHECK(children.IsItemCursor());
  for (NGInlineBackwardCursor cursor(children); cursor;) {
    const NGFragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    if (item->HasSelfPaintingLayer()) {
      cursor.MoveToPreviousSibling();
      continue;
    }

    if (item->IsText()) {
      if (HitTestTextItem(hit_test, *item))
        return true;
    } else if (item->Type() == NGFragmentItem::kLine) {
      const NGPhysicalLineBoxFragment* child_fragment = item->LineBoxFragment();
      DCHECK(child_fragment);
      const PhysicalOffset child_offset =
          hit_test.inline_root_offset + item->OffsetInContainerBlock();
      if (HitTestLineBoxFragment(hit_test, *child_fragment, cursor,
                                 child_offset))
        return true;
    } else if (item->Type() == NGFragmentItem::kBox) {
      if (HitTestChildBoxItem(hit_test, container, *item, cursor))
        return true;
    } else {
      NOTREACHED();
    }

    cursor.MoveToPreviousSibling();

    if (item->Type() != NGFragmentItem::kLine &&
        hit_test.action == kHitTestForeground) {
      // Hit test culled inline boxes between |fragment| and its parent
      // fragment.
      const PhysicalOffset child_offset =
          hit_test.inline_root_offset + item->OffsetInContainerBlock();
      if (HitTestCulledInlineAncestors(*hit_test.result, container, children,
                                       *item, cursor.Current(),
                                       hit_test.location, child_offset))
        return true;
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestFloatingChildren(
    const HitTestContext& hit_test,
    const NGPhysicalContainerFragment& container,
    const PhysicalOffset& accumulated_offset) {
  DCHECK_EQ(hit_test.action, kHitTestFloat);
  DCHECK(container.HasFloatingDescendantsForPaint());

  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(&container)) {
    if (const NGFragmentItems* items = box->Items()) {
      NGInlineCursor children(*items);
      return HitTestFloatingChildItems(hit_test, children, accumulated_offset);
    }
  }

  auto children = container.Children();
  for (const NGLink& child : base::Reversed(children)) {
    const NGPhysicalFragment& child_fragment = *child.fragment;
    if (child_fragment.HasSelfPaintingLayer())
      continue;

    const PhysicalOffset child_offset = accumulated_offset + child.offset;

    if (child_fragment.IsFloating()) {
      if (HitTestAllPhasesInFragment(To<NGPhysicalBoxFragment>(child_fragment),
                                     hit_test.location, child_offset,
                                     hit_test.result))
        return true;
      continue;
    }

    if (child_fragment.IsPaintedAtomically())
      continue;

    const auto* child_container =
        DynamicTo<NGPhysicalContainerFragment>(&child_fragment);
    if (!child_container || !child_container->HasFloatingDescendantsForPaint())
      continue;

    if (child_container->HasOverflowClip()) {
      // We need to properly visit this fragment for hit-testing, rather than
      // jumping directly to its children (which is what we normally do when
      // looking for floats), in order to set up the clip rectangle.
      if (child_container->CanTraverse()) {
        if (NGBoxFragmentPainter(*To<NGPhysicalBoxFragment>(child_container))
                .NodeAtPoint(*hit_test.result, hit_test.location, child_offset,
                             kHitTestFloat))
          return true;
      } else if (child_fragment.GetMutableLayoutObject()->NodeAtPoint(
                     *hit_test.result, hit_test.location, child_offset,
                     kHitTestFloat)) {
        return true;
      }
      continue;
    }

    if (HitTestFloatingChildren(hit_test, *child_container, child_offset))
      return true;
  }
  return false;
}

bool NGBoxFragmentPainter::HitTestFloatingChildItems(
    const HitTestContext& hit_test,
    const NGInlineCursor& children,
    const PhysicalOffset& accumulated_offset) {
  for (NGInlineBackwardCursor cursor(children); cursor;
       cursor.MoveToPreviousSibling()) {
    const NGFragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    if (item->Type() == NGFragmentItem::kBox) {
      if (const NGPhysicalBoxFragment* child_box = item->BoxFragment()) {
        if (child_box->HasSelfPaintingLayer())
          continue;

        const PhysicalOffset child_offset =
            accumulated_offset + item->OffsetInContainerBlock();
        if (child_box->IsFloating()) {
          if (HitTestAllPhasesInFragment(*child_box, hit_test.location,
                                         child_offset, hit_test.result))
            return true;
          continue;
        }

        // Look into descendants of all inline boxes because inline boxes do not
        // have |HasFloatingDescendantsForPaint()| flag.
        if (!child_box->IsInlineBox())
          continue;
      }
      DCHECK(item->GetLayoutObject()->IsLayoutInline());
    } else if (item->Type() == NGFragmentItem::kLine) {
      const NGPhysicalLineBoxFragment* child_line = item->LineBoxFragment();
      DCHECK(child_line);
      if (!child_line->HasFloatingDescendantsForPaint())
        continue;
    } else {
      continue;
    }

    NGInlineCursor descendants = cursor.CursorForDescendants();
    if (HitTestFloatingChildItems(hit_test, descendants, accumulated_offset))
      return true;
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestClippedOutByBorder(
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& border_box_location) const {
  const ComputedStyle& style = box_fragment_.Style();
  PhysicalRect rect(PhysicalOffset(), PhysicalFragment().Size());
  rect.Move(border_box_location);
  const NGBorderEdges& border_edges = BorderEdges();
  return !hit_test_location.Intersects(style.GetRoundedBorderFor(
      rect.ToLayoutRect(), border_edges.line_left, border_edges.line_right));
}

bool NGBoxFragmentPainter::HitTestOverflowControl(
    const HitTestContext& hit_test,
    PhysicalOffset accumulated_offset) {
  const auto* layout_box = ToLayoutBoxOrNull(box_fragment_.GetLayoutObject());
  return layout_box &&
         layout_box->HitTestOverflowControl(*hit_test.result, hit_test.location,
                                            accumulated_offset);
}

}  // namespace blink

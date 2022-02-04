// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fieldset_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_mathml_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_table_painters.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_combine_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/core/paint/url_metadata_utils.h"
#include "third_party/blink/renderer/core/paint/view_painter.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"

namespace blink {

namespace {

inline bool HasSelection(const LayoutObject* layout_object) {
  return layout_object->GetSelectionState() != SelectionState::kNone;
}

inline bool IsVisibleToPaint(const NGPhysicalFragment& fragment,
                             const ComputedStyle& style) {
  if (fragment.IsHiddenForPaint())
    return false;
  if (style.Visibility() != EVisibility::kVisible) {
    auto display = style.Display();
    // Hidden section/row backgrounds still paint into cells.
    if (display != EDisplay::kTableRowGroup && display != EDisplay::kTableRow &&
        display != EDisplay::kTableColumn &&
        display != EDisplay::kTableColumnGroup) {
      return false;
    }
  }

  // When |NGLineTruncator| sets |IsHiddenForPaint|, it sets to the fragment in
  // the line. However, when it has self-painting layer, the fragment stored in
  // |LayoutBlockFlow| will be painted. Check |IsHiddenForPaint| of the fragment
  // in the inline formatting context.
  if (UNLIKELY(fragment.IsAtomicInline() && fragment.HasSelfPaintingLayer())) {
    const LayoutObject* layout_object = fragment.GetLayoutObject();
    if (layout_object->IsInLayoutNGInlineFormattingContext()) {
      NGInlineCursor cursor;
      cursor.MoveTo(*layout_object);
      if (cursor && cursor.Current().IsHiddenForPaint())
        return false;
    }
  }

  return true;
}

inline bool IsVisibleToPaint(const NGFragmentItem& item,
                             const ComputedStyle& style) {
  return !item.IsHiddenForPaint() &&
         style.Visibility() == EVisibility::kVisible;
}

inline bool IsVisibleToHitTest(const ComputedStyle& style,
                               const HitTestRequest& request) {
  return request.IgnorePointerEventsNone() ||
         style.UsedPointerEvents() != EPointerEvents::kNone;
}

inline bool IsVisibleToHitTest(const NGFragmentItem& item,
                               const HitTestRequest& request) {
  const ComputedStyle& style = item.Style();
  if (item.Type() != NGFragmentItem::kSvgText)
    return IsVisibleToPaint(item, style) && IsVisibleToHitTest(style, request);

  if (item.IsHiddenForPaint())
    return false;
  PointerEventsHitRules hit_rules(PointerEventsHitRules::kSvgTextHitTesting,
                                  request, style.UsedPointerEvents());
  if (hit_rules.require_visible && style.Visibility() != EVisibility::kVisible)
    return false;
  if (hit_rules.can_hit_bounding_box ||
      (hit_rules.can_hit_stroke &&
       (style.HasStroke() || !hit_rules.require_stroke)) ||
      (hit_rules.can_hit_fill && (style.HasFill() || !hit_rules.require_fill)))
    return IsVisibleToHitTest(style, request);
  return false;
}

inline bool IsVisibleToHitTest(const NGPhysicalFragment& fragment,
                               const HitTestRequest& request) {
  const ComputedStyle& style = fragment.Style();
  return IsVisibleToPaint(fragment, style) &&
         IsVisibleToHitTest(style, request);
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

  // Check ancestors only when |current| is the first fragment in this line.
  if (previous_sibling && current == previous_sibling.GetLayoutObject())
    return false;

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

    if (auto* parent_layout_inline = DynamicTo<LayoutInline>(parent)) {
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
    const NGPhysicalBoxFragment& container,
    const NGInlineCursor& parent_cursor,
    const NGFragmentItem& item,
    const NGInlineCursorPosition& previous_sibling,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& physical_offset) {
  // Ellipsis can appear under a different parent from the ellipsized object
  // that it can confuse culled inline logic.
  if (UNLIKELY(item.IsEllipsis()))
    return false;
  // To be passed as |accumulated_offset| to LayoutInline::HitTestCulledInline,
  // where it equals the physical offset of the containing block in paint layer.
  const PhysicalOffset fallback_accumulated_offset =
      physical_offset - item.OffsetInContainerFragment();
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
  // If |fragment| is |IsFormattingContextRoot|, it may be legacy.
  // Avoid falling back to |LayoutObject| if |CanTraverse|, because it
  // cannot handle block fragmented objects.
  if (!fragment.IsFormattingContextRoot() || fragment.CanTraverse())
    return false;
  DCHECK(!To<NGPhysicalBoxFragment>(&fragment)->BreakToken());
  return true;
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
    if (const NGFragmentItem* child_item = descendants->CurrentItem()) {
      if (child_item->IsHiddenForPaint())
        continue;
      if (child_item->IsText()) {
        if (child_item->IsLineBreak()) {
          backplates.AddLineBreak();
          continue;
        }

        PhysicalRect box_rect(
            child_item->OffsetInContainerFragment() + paint_offset,
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

  if (!fragment.MayIntersect(*result, hit_test_location, accumulated_offset))
    return false;

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

  if (!fragment.MayIntersect(*result, hit_test_location, accumulated_offset))
    return false;

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

bool ShouldPaintCursorCaret(const NGPhysicalBoxFragment& fragment) {
  return fragment.GetLayoutObject()->GetFrame()->Selection().ShouldPaintCaret(
      fragment);
}

bool ShouldPaintDragCaret(const NGPhysicalBoxFragment& fragment) {
  return fragment.GetLayoutObject()
      ->GetFrame()
      ->GetPage()
      ->GetDragCaret()
      .ShouldPaintCaret(fragment);
}

bool ShouldPaintCarets(const NGPhysicalBoxFragment& fragment) {
  return ShouldPaintCursorCaret(fragment) || ShouldPaintDragCaret(fragment);
}

}  // anonymous namespace

PhysicalRect NGBoxFragmentPainter::InkOverflowIncludingFilters() const {
  if (box_item_)
    return box_item_->SelfInkOverflow();
  const NGPhysicalFragment& fragment = PhysicalFragment();
  DCHECK(!fragment.IsInlineBox());
  return To<LayoutBox>(fragment.GetLayoutObject())
      ->PhysicalVisualOverflowRectIncludingFilters();
}

void NGBoxFragmentPainter::Paint(const PaintInfo& paint_info) {
  if (PhysicalFragment().IsHiddenForPaint())
    return;
  if (PhysicalFragment().IsPaintedAtomically() &&
      !box_fragment_.HasSelfPaintingLayer() &&
      paint_info.phase != PaintPhase::kOverlayOverflowControls) {
    PaintAllPhasesAtomically(paint_info);
  } else {
    PaintInternal(paint_info);
  }
}

void NGBoxFragmentPainter::PaintInternal(const PaintInfo& paint_info) {
  // Avoid initialization of Optional ScopedPaintState::chunk_properties_
  // and ScopedPaintState::adjusted_paint_info_.
  STACK_UNINITIALIZED ScopedPaintState paint_state(box_fragment_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  PaintInfo& info = paint_state.MutablePaintInfo();
  const PhysicalOffset paint_offset = paint_state.PaintOffset();
  const PaintPhase original_phase = info.phase;

  // For text-combine-upright:all, we need to realize canvas here for scaling
  // to fit text content in 1em and shear for "font-style: oblique -15deg".
  absl::optional<DrawingRecorder> recorder;
  absl::optional<GraphicsContextStateSaver> graphics_context_state_saver;
  const auto* const text_combine =
      DynamicTo<LayoutNGTextCombine>(box_fragment_.GetLayoutObject());
  if (UNLIKELY(text_combine)) {
    if (text_combine->NeedsAffineTransformInPaint()) {
      if (original_phase == PaintPhase::kForeground)
        PaintCaretsIfNeeded(paint_state, paint_info, paint_offset);
      if (!paint_info.context.InDrawingRecorder()) {
        if (DrawingRecorder::UseCachedDrawingIfPossible(
                paint_info.context, GetDisplayItemClient(), paint_info.phase))
          return;
        recorder.emplace(paint_info.context, GetDisplayItemClient(),
                         paint_info.phase,
                         text_combine->VisualRectForPaint(paint_offset));
      }
      graphics_context_state_saver.emplace(paint_info.context);
      paint_info.context.ConcatCTM(
          text_combine->ComputeAffineTransformForPaint(paint_offset));
    }
  }

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  if (original_phase == PaintPhase::kForeground &&
      box_fragment_.GetLayoutObject()->IsBox()) {
    scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
        To<LayoutBox>(*box_fragment_.GetLayoutObject()),
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }

  if (original_phase == PaintPhase::kOutline) {
    info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    // We need to call PaintObject twice: one for painting background in the
    // border box space, and the other for painting background in the scrolling
    // contents space.
    auto paint_location = To<LayoutBox>(*box_fragment_.GetLayoutObject())
                              .GetBackgroundPaintLocation();
    if (!(paint_location & kBackgroundPaintInBorderBoxSpace))
      info.SetSkipsBackground(true);
    PaintObject(info, paint_offset);
    info.SetSkipsBackground(false);

    if (paint_location & kBackgroundPaintInContentsSpace) {
      info.SetIsPaintingBackgroundInContentsSpace(true);
      PaintObject(info, paint_offset);
      info.SetIsPaintingBackgroundInContentsSpace(false);
    }
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly &&
      // kOverlayOverflowControls is for the current object itself, so we don't
      // need to traverse descendants here.
      original_phase != PaintPhase::kOverlayOverflowControls) {
    if (original_phase == PaintPhase::kMask ||
        !box_fragment_.GetLayoutObject()->IsBox()) {
      PaintObject(info, paint_offset);
    } else {
      ScopedBoxContentsPaintState contents_paint_state(
          paint_state, To<LayoutBox>(*box_fragment_.GetLayoutObject()));
      PaintObject(contents_paint_state.GetPaintInfo(),
                  contents_paint_state.PaintOffset());
    }
  }

  // If the caret's node's fragment's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  if (original_phase == PaintPhase::kForeground && LIKELY(!recorder)) {
    DCHECK(!text_combine || !text_combine->NeedsAffineTransformInPaint());
    PaintCaretsIfNeeded(paint_state, paint_info, paint_offset);
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    info.phase = PaintPhase::kSelfOutlineOnly;
    PaintObject(info, paint_offset);
  }

  if (UNLIKELY(text_combine) &&
      NGTextCombinePainter::ShouldPaint(*text_combine)) {
    if (recorder) {
      // Paint text decorations and emphasis marks without scaling and share.
      DCHECK(text_combine->NeedsAffineTransformInPaint());
      graphics_context_state_saver->Restore();
    } else if (!paint_info.context.InDrawingRecorder()) {
      if (DrawingRecorder::UseCachedDrawingIfPossible(
              paint_info.context, GetDisplayItemClient(), paint_info.phase))
        return;
      recorder.emplace(paint_info.context, GetDisplayItemClient(),
                       paint_info.phase,
                       text_combine->VisualRectForPaint(paint_offset));
    }
    NGTextCombinePainter::Paint(info, paint_offset, *text_combine);
  }

  // We paint scrollbars after we painted other things, so that the scrollbars
  // will sit above them.
  info.phase = original_phase;
  if (box_fragment_.IsScrollContainer()) {
    DCHECK(!text_combine);
    ScrollableAreaPainter(*PhysicalFragment().Layer()->GetScrollableArea())
        .PaintOverflowControls(info, ToRoundedVector2d(paint_offset));
  }
}

void NGBoxFragmentPainter::RecordScrollHitTestData(
    const PaintInfo& paint_info,
    const DisplayItemClient& background_client) {
  if (!box_fragment_.GetLayoutObject()->IsBox())
    return;
  BoxPainter(To<LayoutBox>(*box_fragment_.GetLayoutObject()))
      .RecordScrollHitTestData(paint_info, background_client);
}

bool NGBoxFragmentPainter::ShouldRecordHitTestData(
    const PaintInfo& paint_info) {
  if (paint_info.IsPaintingBackgroundInContentsSpace() &&
      PhysicalFragment().EffectiveAllowedTouchAction() == TouchAction::kAuto &&
      !PhysicalFragment().InsideBlockingWheelEventHandler()) {
    return false;
  }

  // Hit test data are only needed for compositing. This flag is used for for
  // printing and drag images which do not need hit testing.
  if (paint_info.ShouldOmitCompositingInfo())
    return false;

  // If an object is not visible, it does not participate in hit testing.
  if (PhysicalFragment().Style().Visibility() != EVisibility::kVisible)
    return false;

  // Table rows/sections do not participate in hit testing.
  if (PhysicalFragment().IsTableNGRow() ||
      PhysicalFragment().IsTableNGSection())
    return false;

  return true;
}

void NGBoxFragmentPainter::PaintObject(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    bool suppress_box_decoration_background) {
  const PaintPhase paint_phase = paint_info.phase;
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  const ComputedStyle& style = fragment.Style();
  const bool is_visible = IsVisibleToPaint(fragment, style);
  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    if (is_visible) {
      PaintBoxDecorationBackground(paint_info, paint_offset,
                                   suppress_box_decoration_background);
    }
    // We're done. We don't bother painting any children.
    if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_phase == PaintPhase::kMask && is_visible) {
    PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_phase == PaintPhase::kForeground) {
    // NGBoxFragmentPainter::PaintLineBoxChildren() calls
    // AddURLRectsForInlineChildrenRecursively(). So we don't need to call
    // AddURLRectIfNeeded() for LayoutInline.
    if (paint_info.ShouldAddUrlMetadata()) {
      const auto* layout_object = fragment.GetLayoutObject();
      if (layout_object &&
          (!layout_object->IsLayoutInline() ||
           To<LayoutBoxModelObject>(layout_object)->HasSelfPaintingLayer())) {
        NGFragmentPainter(fragment, GetDisplayItemClient())
            .AddURLRectIfNeeded(paint_info, paint_offset);
      }
    }
    if (is_visible && fragment.HasExtraMathMLPainting())
      NGMathMLPainter(fragment).Paint(paint_info, paint_offset);
  }

  // Paint children.
  if (paint_phase != PaintPhase::kSelfOutlineOnly &&
      (!fragment.Children().empty() || fragment.HasItems() ||
       inline_box_cursor_) &&
      !paint_info.DescendantPaintingBlocked()) {
    if (is_visible && UNLIKELY(paint_phase == PaintPhase::kForeground &&
                               fragment.IsCSSBox() && style.HasColumnRule()))
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
            paint_offset - box_item_->OffsetInContainerFragment();
        PaintInlineItems(paint_info.ForDescendants(),
                         paint_offset_to_inline_formatting_context,
                         box_item_->OffsetInContainerFragment(), &descendants);
      } else if (items_) {
        if (fragment.IsBlockFlow()) {
          PaintBlockFlowContents(paint_info, paint_offset);
        } else {
          DCHECK(fragment.IsInlineBox());
          NGInlineCursor cursor(fragment, *items_);
          PaintInlineItems(paint_info.ForDescendants(), paint_offset,
                           PhysicalOffset(), &cursor);
        }
      } else if (!fragment.IsInlineFormattingContext()) {
        PaintBlockChildren(paint_info, paint_offset);
      }
    }

    if (paint_phase == PaintPhase::kFloat ||
        paint_phase == PaintPhase::kSelectionDragImage ||
        paint_phase == PaintPhase::kTextClip) {
      if (fragment.HasFloatingDescendantsForPaint())
        PaintFloats(paint_info);
    }
  }

  if (!is_visible)
    return;

  // Collapsed borders paint *after* children have painted their backgrounds.
  if (box_fragment_.IsTableNG() &&
      paint_phase == PaintPhase::kDescendantBlockBackgroundsOnly) {
    NGTablePainter(box_fragment_)
        .PaintCollapsedBorders(paint_info, paint_offset,
                               VisualRect(paint_offset));
  }

  if (ShouldPaintSelfOutline(paint_phase)) {
    if (NGOutlineUtils::HasPaintedOutline(style, fragment.GetNode())) {
      NGFragmentPainter(fragment, GetDisplayItemClient())
          .PaintOutline(paint_info, paint_offset, style);
    }
  } else if (ShouldPaintDescendantOutlines(paint_phase)) {
    if (const ComputedStyle* outline_style =
            fragment.StyleForContinuationOutline()) {
      NGFragmentPainter(fragment, GetDisplayItemClient())
          .PaintOutline(paint_info, paint_offset, *outline_style);
    }
  }
}

void NGBoxFragmentPainter::PaintCaretsIfNeeded(
    const ScopedPaintState& paint_state,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  if (!ShouldPaintCarets(box_fragment_))
    return;

  // Apply overflow clip if needed.
  // reveal-caret-of-multiline-contenteditable.html needs this.
  // TDOO(yoisn): We should share this code with |BlockPainter::Paint()|
  absl::optional<ScopedPaintChunkProperties> paint_chunk_properties;
  if (const auto* fragment = paint_state.FragmentToPaint()) {
    if (const auto* properties = fragment->PaintProperties()) {
      if (const auto* overflow_clip = properties->OverflowClip()) {
        paint_chunk_properties.emplace(
            paint_info.context.GetPaintController(), *overflow_clip,
            *box_fragment_.GetLayoutObject(), DisplayItem::kCaret);
      }
    }
  }

  LocalFrame* frame = box_fragment_.GetLayoutObject()->GetFrame();
  if (ShouldPaintCursorCaret(box_fragment_))
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (ShouldPaintDragCaret(box_fragment_)) {
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
  if (!items_ || (layout_object && layout_object->NeedsLayout())) {
    NOTREACHED();
    return;
  }

  // MathML operators paint text (for example enlarged/stretched) content
  // themselves using NGMathMLPainter.
  if (UNLIKELY(fragment.IsMathMLOperator()))
    return;

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
  // TODO(crbug.com/829028): Column boxes do not have |ContentsInkOverflow| atm,
  // hence skip the optimization. If we were to have it, this should be enabled.
  // Otherwise, if we're ok with the perf, we can remove this TODO.
  if (fragment.IsCSSBox()) {
    PhysicalRect content_ink_rect = fragment.LocalRect();
    content_ink_rect.Unite(fragment.ContentsInkOverflow());
    if (!paint_info.IntersectsCullRect(content_ink_rect, paint_offset))
      return;
  }

  DCHECK(items_);
  NGInlineCursor children(fragment, *items_);
  if (fragment.IsSvgText()) {
    ScopedSVGPaintState paint_state(*fragment.GetLayoutObject(), paint_info);
    PaintLineBoxChildren(&children, paint_info.ForDescendants(), paint_offset);
    return;
  }
  PaintLineBoxChildren(&children, paint_info.ForDescendants(), paint_offset);
}

void NGBoxFragmentPainter::PaintBlockChildren(const PaintInfo& paint_info,
                                              PhysicalOffset paint_offset) {
  DCHECK(!box_fragment_.IsInlineFormattingContext());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  paint_info_for_descendants.SetIsInFragmentTraversal();
  for (const NGLink& child : box_fragment_.Children()) {
    const NGPhysicalFragment& child_fragment = *child;
    DCHECK(child_fragment.IsBox());
    if (child_fragment.HasSelfPaintingLayer() || child_fragment.IsFloating())
      continue;
    PaintBlockChild(child, paint_info, paint_info_for_descendants,
                    paint_offset);
  }
}

void NGBoxFragmentPainter::PaintBlockChild(
    const NGLink& child,
    const PaintInfo& paint_info,
    const PaintInfo& paint_info_for_descendants,
    PhysicalOffset paint_offset) {
  const NGPhysicalFragment& child_fragment = *child;
  DCHECK(child_fragment.IsBox());
  DCHECK(!child_fragment.HasSelfPaintingLayer());
  DCHECK(!child_fragment.IsFloating());
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

      if (box_child_fragment.IsFragmentainerBox()) {
        // This is a fragmentainer, and when node inside a fragmentation
        // context paints multiple block fragments, we need to distinguish
        // between them somehow, for paint caching to work. Therefore,
        // establish a display item scope here.
        unsigned identifier = FragmentainerUniqueIdentifier(box_child_fragment);
        ScopedDisplayItemFragment scope(paint_info.context, identifier);
        NGBoxFragmentPainter(box_child_fragment)
            .PaintObject(paint_info, child_offset);
        return;
      }

      NGBoxFragmentPainter(box_child_fragment)
          .PaintObject(paint_info, child_offset);
      return;
    }

    NGBoxFragmentPainter(box_child_fragment).Paint(paint_info_for_descendants);
    return;
  }

  // Fall back to flow-thread painting when reaching a column (the flow thread
  // is treated as a self-painting PaintLayer when fragment traversal is
  // disabled, so nothing to do here).
  if (box_child_fragment.IsColumnBox())
    return;

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

void NGBoxFragmentPainter::PaintFloatingItems(const PaintInfo& paint_info,
                                              const PaintInfo& float_paint_info,
                                              NGInlineCursor* cursor) {
  while (*cursor) {
    const NGFragmentItem* item = cursor->Current().Item();
    DCHECK(item);
    const NGPhysicalBoxFragment* child_fragment = item->BoxFragment();
    if (!child_fragment) {
      cursor->MoveToNext();
      continue;
    }
    if (child_fragment->HasSelfPaintingLayer()) {
      cursor->MoveToNextSkippingChildren();
      continue;
    }
    if (child_fragment->IsFloating()) {
      if (child_fragment->CanTraverse()) {
        NGBoxFragmentPainter(*child_fragment).Paint(float_paint_info);
      } else {
        ObjectPainter(*child_fragment->GetLayoutObject())
            .PaintAllPhasesAtomically(float_paint_info);
      }
    } else if (child_fragment->IsBlockInInline() &&
               child_fragment->HasFloatingDescendantsForPaint()) {
      NGBoxFragmentPainter(*child_fragment).Paint(paint_info);
    }
    DCHECK(child_fragment->IsInlineBox() || !cursor->Current().HasChildren());
    cursor->MoveToNext();
  }
}

void NGBoxFragmentPainter::PaintFloatingChildren(
    const NGPhysicalFragment& container,
    const PaintInfo& paint_info) {
  DCHECK(container.HasFloatingDescendantsForPaint());

  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;
  PaintFloatingChildren(container, paint_info, float_paint_info);
}

void NGBoxFragmentPainter::PaintFloatingChildren(
    const NGPhysicalFragment& container,
    const PaintInfo& paint_info,
    const PaintInfo& float_paint_info) {
  DCHECK(container.HasFloatingDescendantsForPaint());

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

      // Drawing in SelectionDragImage phase can result in an exponential
      // paint time: crbug.com://1182106
      if (paint_info.phase != PaintPhase::kSelectionDragImage &&
          child_fragment.Type() == NGPhysicalFragment::kFragmentBox &&
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

    if (!child_fragment.HasFloatingDescendantsForPaint())
      continue;

    if (child_fragment.HasNonVisibleOverflow()) {
      // We need to properly visit this fragment for painting, rather than
      // jumping directly to its children (which is what we normally do when
      // looking for floats), in order to set up the clip rectangle.
      NGBoxFragmentPainter(To<NGPhysicalBoxFragment>(child_fragment))
          .Paint(paint_info);
      continue;
    }

    if (child_fragment.IsFragmentainerBox()) {
      // This is a fragmentainer, and when node inside a fragmentation context
      // paints multiple block fragments, we need to distinguish between them
      // somehow, for paint caching to work. Therefore, establish a display item
      // scope here.
      unsigned identifier = FragmentainerUniqueIdentifier(
          To<NGPhysicalBoxFragment>(child_fragment));
      ScopedDisplayItemFragment scope(paint_info.context, identifier);
      PaintFloatingChildren(child_fragment, paint_info, float_paint_info);
    } else {
      PaintFloatingChildren(child_fragment, paint_info, float_paint_info);
    }
  }

  // Now process the inline formatting context, if any. Note that even if this
  // is an inline formatting context, we still need to walk the box fragment
  // children (like we did above). If a float is block-fragmented, it is resumed
  // as a regular box fragment child, rather than becoming a fragment item.
  if (const NGPhysicalBoxFragment* box =
          DynamicTo<NGPhysicalBoxFragment>(&container)) {
    if (const NGFragmentItems* items = box->Items()) {
      NGInlineCursor cursor(*box, *items);
      PaintFloatingItems(paint_info, float_paint_info, &cursor);
      return;
    }
    if (inline_box_cursor_) {
      DCHECK(box->IsInlineBox());
      NGInlineCursor descendants = inline_box_cursor_->CursorForDescendants();
      PaintFloatingItems(paint_info, float_paint_info, &descendants);
      return;
    }
    DCHECK(!box->IsInlineBox());
  }
}

void NGBoxFragmentPainter::PaintFloats(const PaintInfo& paint_info) {
  DCHECK(PhysicalFragment().HasFloatingDescendantsForPaint() ||
         !PhysicalFragment().IsInlineFormattingContext());
  PaintFloatingChildren(PhysicalFragment(), paint_info);
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

  if (physical_box_fragment.IsFieldsetContainer()) {
    NGFieldsetPainter(box_fragment_).PaintMask(paint_info, paint_offset);
    return;
  }

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));

  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           paint_info.phase, VisualRect(paint_offset));
  PhysicalRect paint_rect(paint_offset, box_fragment_.Size());
  PaintMaskImages(paint_info, paint_rect, *box_fragment_.GetLayoutObject(),
                  geometry, box_fragment_.SidesToInclude());
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    bool suppress_box_decoration_background) {
  // TODO(mstensho): Break dependency on LayoutObject functionality.
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();

  if (const auto* view = DynamicTo<LayoutView>(&layout_object)) {
    ViewPainter(*view).PaintBoxDecorationBackground(paint_info);
    return;
  }

  PhysicalRect paint_rect;
  const DisplayItemClient* background_client = nullptr;
  absl::optional<ScopedBoxContentsPaintState> contents_paint_state;
  gfx::Rect visual_rect;
  if (paint_info.IsPaintingBackgroundInContentsSpace()) {
    // For the case where we are painting the background in the contents space,
    // we need to include the entire overflow rect.
    const LayoutBox& layout_box = To<LayoutBox>(layout_object);
    paint_rect = layout_box.PhysicalLayoutOverflowRect();

    contents_paint_state.emplace(paint_info, paint_offset, layout_box);
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.Expand(layout_box.BorderBoxOutsets());

    background_client = &layout_box.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
    visual_rect = layout_box.GetScrollableArea()->ScrollingBackgroundVisualRect(
        paint_offset);
  } else {
    paint_rect.offset = paint_offset;
    paint_rect.size = box_fragment_.Size();
    if (layout_object.IsTableCell() && !box_fragment_.IsTableNGCell()) {
      paint_rect.size =
          PhysicalSize(To<LayoutBox>(layout_object).PixelSnappedSize());
    }
    background_client = &GetDisplayItemClient();
    visual_rect = VisualRect(paint_offset);
  }

  if (!suppress_box_decoration_background) {
    PaintBoxDecorationBackgroundWithRect(
        contents_paint_state ? contents_paint_state->GetPaintInfo()
                             : paint_info,
        visual_rect, paint_rect, *background_client);
  }

  if (ShouldRecordHitTestData(paint_info)) {
    paint_info.context.GetPaintController().RecordHitTestData(
        *background_client, ToPixelSnappedRect(paint_rect),
        PhysicalFragment().EffectiveAllowedTouchAction(),
        PhysicalFragment().InsideBlockingWheelEventHandler());
  }

  Element* element = DynamicTo<Element>(layout_object.GetNode());
  if (element && element->GetRegionCaptureCropId()) {
    paint_info.context.GetPaintController().RecordRegionCaptureData(
        *background_client, *(element->GetRegionCaptureCropId()),
        ToPixelSnappedRect(paint_rect));
  }

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (!paint_info.IsPaintingBackgroundInContentsSpace())
    RecordScrollHitTestData(paint_info, *background_client);
}

void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const gfx::Rect& visual_rect,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  BoxDecorationData box_decoration_data(paint_info, box_fragment_);
  if (!box_decoration_data.ShouldPaint() &&
      (!box_fragment_.IsTableNG() ||
       !NGTablePainter(box_fragment_).WillCheckColumnBackgrounds())) {
    return;
  }

  absl::optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      ShouldSkipPaintUnderInvalidationChecking(
          To<LayoutBox>(*box_fragment_.GetLayoutObject()))) {
    cache_skipper.emplace(paint_info.context);
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, background_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, background_client,
                           DisplayItem::kBoxDecorationBackground, visual_rect);

  if (PhysicalFragment().IsFieldsetContainer()) {
    NGFieldsetPainter(box_fragment_)
        .PaintBoxDecorationBackground(paint_info, paint_rect,
                                      box_decoration_data);
  } else if (PhysicalFragment().IsTableNGPart()) {
    if (box_fragment_.IsTableNGCell()) {
      NGTableCellPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    } else if (box_fragment_.IsTableNGRow()) {
      NGTableRowPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    } else if (box_fragment_.IsTableNGSection()) {
      NGTableSectionPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    } else {
      DCHECK(box_fragment_.IsTableNG());
      NGTablePainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    }
  } else {
    PaintBoxDecorationBackgroundWithRectImpl(paint_info, paint_rect,
                                             box_decoration_data);
  }
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRectImpl(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = To<LayoutBox>(layout_object);

  const ComputedStyle& style = box_fragment_.Style();

  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (box_decoration_data.ShouldPaintShadow()) {
    PaintNormalBoxShadow(paint_info, paint_rect, style,
                         box_fragment_.SidesToInclude(),
                         !box_decoration_data.ShouldPaintBackground());
  }

  bool needs_end_layer = false;
  if (!box_decoration_data.IsPaintingBackgroundInContentsSpace()) {
    if (box_fragment_.HasSelfPaintingLayer() && layout_box.IsTableCell() &&
        ToInterface<LayoutNGTableCellInterface>(layout_box)
            .TableInterface()
            ->ShouldCollapseBorders()) {
      // TODO(crbug.com/1081425) This branch is only used by Legacy
      // tables. Remove when Legacy tables are removed.
      // We have to clip here because the background would paint on top of the
      // collapsed table borders otherwise, since this is a self-painting layer.
      PhysicalRect clip_rect = paint_rect;
      clip_rect.Expand(layout_box.BorderInsets());
      state_saver.Save();
      paint_info.context.Clip(ToPixelSnappedRect(clip_rect));
    } else if (BleedAvoidanceIsClipping(
                   box_decoration_data.GetBackgroundBleedAvoidance())) {
      state_saver.Save();
      FloatRoundedRect border =
          RoundedBorderGeometry::PixelSnappedRoundedBorder(
              style, paint_rect, box_fragment_.SidesToInclude());
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.GetBackgroundBleedAvoidance() ==
          kBackgroundBleedClipLayer) {
        paint_info.context.BeginLayer();
        needs_end_layer = true;
      }
    }
  }

  gfx::Rect snapped_paint_rect = ToPixelSnappedRect(paint_rect);
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
                                        box_fragment_.SidesToInclude());
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
                  box_fragment_.SidesToInclude());
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

void NGBoxFragmentPainter::PaintBoxDecorationBackgroundForBlockInInline(
    NGInlineCursor* children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  while (*children) {
    const NGFragmentItem* item = children->Current().Item();
    if (const NGPhysicalLineBoxFragment* line = item->LineBoxFragment()) {
      if (!line->IsBlockInInline()) {
        children->MoveToNextSkippingChildren();
        continue;
      }
    } else if (const NGPhysicalBoxFragment* fragment = item->BoxFragment()) {
      if (fragment->HasSelfPaintingLayer()) {
        children->MoveToNextSkippingChildren();
        continue;
      }
      if (fragment->IsBlockInInline())
        PaintBoxItem(*item, *fragment, *children, paint_info, paint_offset);
    }
    children->MoveToNext();
  }
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
                           DisplayItem::kColumnRules, gfx::Rect());

  const Color& rule_color =
      LayoutObject::ResolveColor(style, GetCSSPropertyColumnRuleColor());
  LayoutUnit rule_thickness(style.ColumnRuleWidth());
  PhysicalRect previous_column;
  bool past_first_column_in_row = false;
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
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
    gfx::Rect snapped_rule = ToPixelSnappedRect(rule);
    BoxBorderPainter::DrawBoxSide(paint_info.context, snapped_rule, box_side,
                                  rule_color, rule_style, auto_dark_mode);
    recorder.UniteVisualRect(snapped_rule);

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
  const auto& layout_box = To<LayoutBox>(*box_fragment_.GetLayoutObject());
  if (layout_box.BackgroundTransfersToView())
    return;
  if (layout_box.BackgroundIsKnownToBeObscured())
    return;

  BackgroundImageGeometry geometry(box_fragment_);
  PaintFillLayers(paint_info, background_color,
                  box_fragment_.Style().BackgroundLayers(), paint_rect,
                  geometry, bleed_avoidance);
}

void NGBoxFragmentPainter::PaintInlineChildBoxUsingLegacyFallback(
    const NGPhysicalFragment& fragment,
    const PaintInfo& paint_info) {
  const LayoutObject* child_layout_object = fragment.GetLayoutObject();
  DCHECK(child_layout_object);

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
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      // TODO(crbug.com/1099613): This should not happen, as long as it is
      // really layout-clean.
      NOTREACHED();
      cursor->MoveToNextSkippingChildren();
      continue;
    }
    switch (item->Type()) {
      case NGFragmentItem::kText:
      case NGFragmentItem::kSvgText:
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
    const NGFragmentItem& line_box_item,
    const PaintInfo& paint_info,
    const PhysicalOffset& child_offset) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  PhysicalRect border_box = line_box_fragment.LocalRect();
  border_box.offset += child_offset;
  absl::optional<ScopedDisplayItemFragment> display_item_fragment;
  const wtf_size_t line_fragment_id = line_box_item.FragmentId();
  DCHECK_GE(line_fragment_id, NGFragmentItem::kInitialLineFragmentId);
  if (ShouldRecordHitTestData(paint_info)) {
    display_item_fragment.emplace(paint_info.context, line_fragment_id);
    paint_info.context.GetPaintController().RecordHitTestData(
        display_item_client, ToPixelSnappedRect(border_box),
        PhysicalFragment().EffectiveAllowedTouchAction(),
        PhysicalFragment().InsideBlockingWheelEventHandler());
  }

  Element* element = DynamicTo<Element>(line_box_fragment.GetNode());
  if (element && element->GetRegionCaptureCropId()) {
    paint_info.context.GetPaintController().RecordRegionCaptureData(
        display_item_client, *(element->GetRegionCaptureCropId()),
        ToPixelSnappedRect(border_box));
  }

  // Paint the background of the `::first-line` line box.
  if (NGLineBoxFragmentPainter::NeedsPaint(line_box_fragment)) {
    if (!display_item_fragment)
      display_item_fragment.emplace(paint_info.context, line_fragment_id);
    NGLineBoxFragmentPainter line_box_painter(line_box_fragment, line_box_item,
                                              PhysicalFragment());
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
      paint_info.phase != PaintPhase::kOutline) {
    if (UNLIKELY(ShouldPaintDescendantBlockBackgrounds(paint_info.phase))) {
      // When block-in-inline, block backgrounds need to be painted.
      PaintBoxDecorationBackgroundForBlockInInline(children, paint_info,
                                                   paint_offset);
    }
    return;
  }

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

  DCHECK(children->HasRoot());
  PaintLineBoxChildItems(children, paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintLineBoxChildItems(
    NGInlineCursor* children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const bool is_horizontal = box_fragment_.Style().IsHorizontalWritingMode();
  for (; *children; children->MoveToNextSkippingChildren()) {
    const NGFragmentItem* child_item = children->CurrentItem();
    DCHECK(child_item);
    if (child_item->IsFloating())
      continue;

    // Check if CullRect intersects with this child, only in block direction
    // because soft-wrap and <br> needs to paint outside of InkOverflow() in
    // inline direction.
    const PhysicalOffset& child_offset =
        paint_offset + child_item->OffsetInContainerFragment();
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
      PaintLineBox(*line_box_fragment, *child_item->GetDisplayItemClient(),
                   *child_item, paint_info, child_offset);
      NGInlineCursor line_box_cursor = children->CursorForDescendants();
      PaintInlineItems(paint_info, paint_offset,
                       child_item->OffsetInContainerFragment(),
                       &line_box_cursor);
      continue;
    }

    if (const NGPhysicalBoxFragment* child_fragment =
            child_item->BoxFragment()) {
      DCHECK(!child_fragment->IsOutOfFlowPositioned());
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

  // Only paint backplates behind text when forced-color-adjust is auto and the
  // element is visible.
  const ComputedStyle& style = PhysicalFragment().Style();
  if (style.ForcedColorAdjust() != EForcedColorAdjust::kAuto ||
      style.Visibility() != EVisibility::kVisible)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, GetDisplayItemClient(),
          DisplayItem::kForcedColorsModeBackplate))
    return;

  Color backplate_color = PhysicalFragment()
                              .GetLayoutObject()
                              ->GetDocument()
                              .GetStyleEngine()
                              .ForcedBackgroundColor();
  const auto& backplates = BuildBackplate(line_boxes, paint_offset);
  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           DisplayItem::kForcedColorsModeBackplate,
                           ToEnclosingRect(UnionRect(backplates)));
  for (const auto backplate : backplates) {
    paint_info.context.FillRect(
        gfx::RectF(backplate), backplate_color,
        PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  }
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
          item.InkOverflow(),
          paint_offset + item.OffsetInContainerFragment()) &&
      // Don't skip <br>, it doesn't have ink but need to paint selection.
      !(item.IsLineBreak() && HasSelection(item.GetLayoutObject())))
    return;

  ScopedDisplayItemFragment display_item_fragment(paint_info.context,
                                                  item.FragmentId());
  NGTextFragmentPainter text_painter(cursor, parent_offset);
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
  DCHECK_EQ(item.PostLayoutBoxFragment(), &child_fragment);
  DCHECK(!child_fragment.IsHiddenForPaint());
  if (child_fragment.HasSelfPaintingLayer() || child_fragment.IsFloating())
    return;

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          child_fragment.InkOverflow(),
          paint_offset + item.OffsetInContainerFragment()))
    return;

  if (child_fragment.IsAtomicInline() || child_fragment.IsListMarker()) {
    if (FragmentRequiresLegacyFallback(child_fragment)) {
      // The legacy painter may be confused when painting fragmented descendant
      // PaintLayers (which should not be fragmented but legacy layout does) and
      // will produce duplicated PaintChunk ids for the fragments. Skip display
      // item cache to tolerate that.
      absl::optional<DisplayItemCacheSkipper> skipper;
      if (paint_info.context.GetPaintController().CurrentFragment())
        skipper.emplace(paint_info.context);
      PaintInlineChildBoxUsingLegacyFallback(child_fragment, paint_info);
      return;
    }
    NGBoxFragmentPainter(child_fragment).PaintAllPhasesAtomically(paint_info);
    return;
  }

  if (child_fragment.IsInlineBox()) {
    NGInlineBoxFragmentPainter(cursor, item, child_fragment)
        .Paint(paint_info, paint_offset);
    return;
  }

  // Block-in-inline
  DCHECK(RuntimeEnabledFeatures::LayoutNGBlockInInlineEnabled());
  DCHECK(!child_fragment.GetLayoutObject()->IsInline());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  paint_info_for_descendants.SetIsInFragmentTraversal();
  PaintBlockChild({&child_fragment, item.OffsetInContainerFragment()},
                  paint_info, paint_info_for_descendants, paint_offset);
}

void NGBoxFragmentPainter::PaintBoxItem(const NGFragmentItem& item,
                                        const NGInlineCursor& cursor,
                                        const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset,
                                        const PhysicalOffset& parent_offset) {
  DCHECK_EQ(item.Type(), NGFragmentItem::kBox);
  DCHECK_EQ(&item, cursor.Current().Item());

  if (const NGPhysicalBoxFragment* child_fragment = item.BoxFragment()) {
    child_fragment = child_fragment->PostLayout();
    if (child_fragment)
      PaintBoxItem(item, *child_fragment, cursor, paint_info, paint_offset);
    return;
  }

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          item.InkOverflow(), paint_offset + item.OffsetInContainerFragment()))
    return;

  // This |item| is a culled inline box.
  DCHECK(item.GetLayoutObject()->IsLayoutInline());
  NGInlineCursor children = cursor.CursorForDescendants();
  // Pass the given |parent_offset| because culled inline boxes do not affect
  // the sub-pixel snapping behavior. TODO(kojii): This is for the
  // compatibility, we may want to revisit in future.
  PaintInlineItems(paint_info, paint_offset, parent_offset, &children);
}

bool NGBoxFragmentPainter::ShouldPaint(
    const ScopedPaintState& paint_state) const {
  // TODO(layout-dev): Add support for scrolling, see BlockPainter::ShouldPaint.
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  if (!fragment.IsInlineBox()) {
    return paint_state.LocalRectIntersectsCullRect(
        To<LayoutBox>(fragment.GetLayoutObject())
            ->PhysicalVisualOverflowRect());
  }
  NOTREACHED();
  return false;
}

void NGBoxFragmentPainter::PaintTextClipMask(const PaintInfo& paint_info,
                                             const gfx::Rect& mask_rect,
                                             const PhysicalOffset& paint_offset,
                                             bool object_has_multiple_boxes) {
  PaintInfo mask_paint_info(paint_info.context, CullRect(mask_rect),
                            PaintPhase::kTextClip);
  mask_paint_info.SetFragmentID(paint_info.FragmentID());
  if (!object_has_multiple_boxes) {
    PaintObject(mask_paint_info, paint_offset);
    return;
  }

  DCHECK(inline_box_cursor_);
  DCHECK(box_item_);
  NGInlineBoxFragmentPainter inline_box_painter(*inline_box_cursor_,
                                                *box_item_);
  PaintTextClipMask(mask_paint_info,
                    paint_offset - box_item_->OffsetInContainerFragment(),
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
      !paint_info.IsPaintingBackgroundInContentsSpace()) {
    context.Clip(gfx::RectF(physical.OverflowClipRect(rect.offset)));

    // Adjust the paint rect to reflect a scrolled content box with borders at
    // the ends.
    scrolled_paint_rect.offset -=
        PhysicalOffset(physical.PixelSnappedScrolledContentOffset());
    LayoutRectOutsets borders = AdjustedBorderOutsets(info);
    scrolled_paint_rect.size =
        physical.ScrollSize() + PhysicalSize(borders.Size());
  }
  return scrolled_paint_rect;
}

LayoutRectOutsets NGBoxFragmentPainter::ComputeBorders() const {
  if (box_fragment_.GetLayoutObject()->IsTableCellLegacy())
    return To<LayoutBox>(box_fragment_.GetLayoutObject())->BorderBoxOutsets();
  return PhysicalFragment().Borders().ToLayoutRectOutsets();
}

LayoutRectOutsets NGBoxFragmentPainter::ComputePadding() const {
  return PhysicalFragment().Padding().ToLayoutRectOutsets();
}

BoxPainterBase::FillLayerInfo NGBoxFragmentPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_background_in_contents_space) const {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  RespectImageOrientationEnum respect_orientation =
      LayoutObject::ShouldRespectImageOrientation(fragment.GetLayoutObject());
  if (auto* style_image = bg_layer.GetImage()) {
    respect_orientation =
        style_image->ForceOrientationIfNecessary(respect_orientation);
  }
  return BoxPainterBase::FillLayerInfo(
      fragment.GetLayoutObject()->GetDocument(), fragment.Style(),
      fragment.IsScrollContainer(), color, bg_layer, bleed_avoidance,
      respect_orientation, box_fragment_.SidesToInclude(),
      fragment.GetLayoutObject()->IsLayoutInline(),
      is_painting_background_in_contents_space);
}

template <typename T>
bool NGBoxFragmentPainter::HitTestContext::AddNodeToResult(
    Node* node,
    const NGPhysicalBoxFragment* box_fragment,
    const T& bounds_rect,
    const PhysicalOffset& offset) const {
  if (node && !result->InnerNode())
    result->SetNodeAndPosition(node, box_fragment, location.Point() - offset);
  return result->AddNodeToListBasedTestResult(node, location, bounds_rect) ==
         kStopHitTesting;
}

template <typename T>
bool NGBoxFragmentPainter::HitTestContext::AddNodeToResultWithContentOffset(
    Node* node,
    const NGPhysicalBoxFragment& container,
    const T& bounds_rect,
    PhysicalOffset offset) const {
  if (container.IsScrollContainer())
    offset += PhysicalOffset(container.PixelSnappedScrolledContentOffset());
  return AddNodeToResult(node, &container, bounds_rect, offset);
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
  // TODO(mstensho): Make sure that we never create an NGBoxFragmentPainter for
  // a fragment that doesn't intersect, and turn this into a DCHECK.
  if (!fragment.MayIntersect(*hit_test.result, hit_test.location,
                             physical_offset))
    return false;

  bool pointer_events_bounding_box = false;
  bool hit_test_self = fragment.IsInSelfHitTestingPhase(hit_test.action);
  if (hit_test_self) {
    // Table row and table section are never a hit target.
    // SVG <text> is not a hit target except if 'pointer-events: bounding-box'.
    if (PhysicalFragment().IsTableNGRow() ||
        PhysicalFragment().IsTableNGSection()) {
      hit_test_self = false;
    } else if (fragment.IsSvgText()) {
      pointer_events_bounding_box =
          fragment.Style().UsedPointerEvents() == EPointerEvents::kBoundingBox;
      hit_test_self = pointer_events_bounding_box;
    }
  }

  if (hit_test_self && box_fragment_.IsScrollContainer() &&
      HitTestOverflowControl(hit_test, physical_offset))
    return true;

  const PhysicalSize& size = fragment.Size();
  const ComputedStyle& style = fragment.Style();
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  bool skip_children =
      layout_object &&
      (layout_object == hit_test.result->GetHitTestRequest().GetStopNode() ||
       layout_object->ChildPaintBlockedByDisplayLock());
  if (!skip_children && box_fragment_.ShouldClipOverflowAlongEitherAxis()) {
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
          RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(style,
                                                                bounds_rect));
    }
  }

  if (!skip_children) {
    if (!box_fragment_.IsScrollContainer()) {
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
      IsVisibleToHitTest(box_fragment_, hit_test.result->GetHitTestRequest())) {
    PhysicalRect bounds_rect(physical_offset, size);
    if (UNLIKELY(
            hit_test.result->GetHitTestRequest().IsHitTestVisualOverflow())) {
      // We'll include overflow from children here (in addition to self-overflow
      // caused by filters), because we want to record a match if we hit the
      // overflow of a child below the stop node. This matches legacy behavior
      // in LayoutBox::NodeAtPoint(); see call to
      // PhysicalVisualOverflowRectIncludingFilters().
      bounds_rect = InkOverflowIncludingFilters();
      bounds_rect.Move(physical_offset);
    }
    if (UNLIKELY(pointer_events_bounding_box)) {
      bounds_rect = PhysicalRect::EnclosingRect(
          PhysicalFragment().GetLayoutObject()->ObjectBoundingBox());
    }
    // TODO(kojii): Don't have good explanation why only inline box needs to
    // snap, but matches to legacy and fixes crbug.com/976606.
    if (fragment.IsInlineBox())
      bounds_rect = PhysicalRect(ToPixelSnappedRect(bounds_rect));
    if (hit_test.location.Intersects(bounds_rect)) {
      // We set offset in container block instead of offset in |fragment| like
      // |NGBoxFragmentPainter::HitTestTextFragment()|.
      // See http://crbug.com/1043471
      if (box_item_ && box_item_->IsInlineBox()) {
        DCHECK(inline_box_cursor_);
        if (hit_test.AddNodeToResultWithContentOffset(
                fragment.NodeForHitTest(),
                inline_box_cursor_->ContainerFragment(), bounds_rect,
                physical_offset - box_item_->OffsetInContainerFragment()))
          return true;
      } else {
        if (hit_test.AddNodeToResult(fragment.NodeForHitTest(), &box_fragment_,
                                     bounds_rect, physical_offset))
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
  // TODO(mstensho): Make sure that we never create an NGBoxFragmentPainter for
  // a fragment that doesn't intersect, and DCHECK for that here.

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

bool NGBoxFragmentPainter::HitTestTextItem(
    const HitTestContext& hit_test,
    const NGFragmentItem& text_item,
    const NGInlineBackwardCursor& cursor) {
  DCHECK(text_item.IsText());

  if (hit_test.action != kHitTestForeground)
    return false;
  if (!IsVisibleToHitTest(text_item, hit_test.result->GetHitTestRequest()))
    return false;

  if (text_item.Type() == NGFragmentItem::kSvgText &&
      text_item.HasSvgTransformForBoundingBox()) {
    const gfx::QuadF quad = text_item.SvgUnscaledQuad();
    if (!hit_test.location.Intersects(quad))
      return false;
    return hit_test.AddNodeToResultWithContentOffset(
        text_item.NodeForHitTest(), cursor.ContainerFragment(), quad,
        hit_test.inline_root_offset);
  }

  const auto* const text_combine =
      DynamicTo<LayoutNGTextCombine>(box_fragment_.GetLayoutObject());

  // TODO(layout-dev): Clip to line-top/bottom.
  const PhysicalRect rect =
      UNLIKELY(text_combine)
          ? text_combine->ComputeTextBoundsRectForHitTest(
                text_item, hit_test.inline_root_offset)
          : text_item.ComputeTextBoundsRectForHitTest(
                hit_test.inline_root_offset,
                hit_test.result->GetHitTestRequest().IsHitTestVisualOverflow());
  if (!hit_test.location.Intersects(rect))
    return false;

  return hit_test.AddNodeToResultWithContentOffset(
      text_item.NodeForHitTest(), cursor.ContainerFragment(), rect,
      hit_test.inline_root_offset);
}

// Replicates logic in legacy InlineFlowBox::NodeAtPoint().
bool NGBoxFragmentPainter::HitTestLineBoxFragment(
    const HitTestContext& hit_test,
    const NGPhysicalLineBoxFragment& fragment,
    const NGInlineBackwardCursor& cursor,
    const PhysicalOffset& physical_offset) {
  PhysicalRect overflow_rect = cursor.Current().InkOverflow();
  overflow_rect.Move(physical_offset);
  if (!hit_test.location.Intersects(overflow_rect))
    return false;

  if (HitTestChildren(hit_test, PhysicalFragment(),
                      cursor.CursorForDescendants(), physical_offset))
    return true;

  if (hit_test.action != kHitTestForeground)
    return false;

  if (!IsVisibleToHitTest(box_fragment_, hit_test.result->GetHitTestRequest()))
    return false;

  const PhysicalOffset overflow_location =
      cursor.Current().SelfInkOverflow().offset + physical_offset;
  if (HitTestClippedOutByBorder(hit_test.location, overflow_location))
    return false;

  const PhysicalRect bounds_rect(physical_offset, fragment.Size());
  const ComputedStyle& containing_box_style = box_fragment_.Style();
  if (containing_box_style.HasBorderRadius() &&
      !hit_test.location.Intersects(
          RoundedBorderGeometry::PixelSnappedRoundedBorder(containing_box_style,
                                                           bounds_rect)))
    return false;

  if (cursor.ContainerFragment().IsSvgText())
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

  // |physical_offset| is inside line, but
  //  * Outside of children
  //  * In child without no foreground descendant, e.g. block with size.
  if (cursor.Current()->LineBoxFragment()->IsBlockInInline()) {
    // "fast/events/ondragenter.html" reaches here.
    return false;
  }

  return hit_test.AddNodeToResultWithContentOffset(
      fragment.NodeForHitTest(), box_fragment_, bounds_rect,
      physical_offset - cursor.Current().OffsetInContainerFragment());
}

bool NGBoxFragmentPainter::HitTestChildBoxFragment(
    const HitTestContext& hit_test,
    const NGPhysicalBoxFragment& fragment,
    const NGInlineBackwardCursor& backward_cursor,
    const PhysicalOffset& physical_offset) {
  bool is_in_atomic_painting_pass;

  // Note: Floats should only be hit tested in the |kHitTestFloat| phase, so we
  // shouldn't enter a float when |action| doesn't match. However, as floats may
  // scatter around in the entire inline formatting context, we should always
  // enter non-floating inline child boxes to search for floats in the
  // |kHitTestFloat| phase, unless the child box forms another context.
  if (fragment.IsFloating()) {
    if (hit_test.action != kHitTestFloat)
      return false;
    is_in_atomic_painting_pass = true;
  } else {
    is_in_atomic_painting_pass = hit_test.action == kHitTestForeground;
  }

  if (!FragmentRequiresLegacyFallback(fragment)) {
    if (fragment.IsPaintedAtomically()) {
      if (!is_in_atomic_painting_pass)
        return false;
      return HitTestAllPhasesInFragment(fragment, hit_test.location,
                                        physical_offset, hit_test.result);
    }
    NGInlineCursor cursor(backward_cursor);
    const NGFragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    DCHECK_EQ(item->BoxFragment(), &fragment);
    if (!fragment.MayIntersect(*hit_test.result, hit_test.location,
                               physical_offset))
      return false;

    if (fragment.IsInlineBox()) {
      return NGBoxFragmentPainter(cursor, *item, fragment)
          .NodeAtPoint(hit_test, physical_offset);
    }

    if (fragment.IsBlockInInline()) {
      if (NGBoxFragmentPainter(fragment).NodeAtPoint(hit_test,
                                                     physical_offset)) {
        return true;
      }
      if (!box_fragment_.IsInlineBox()) {
        // fast/events/pointerevents/mouse-pointer-transition-events.html
        // requires this.
        return false;
      }
      // [1] and [2] reach here for hit test on empty <div> with size.
      // [1] label-contains-other-interactive-content.html
      // [2] svg/custom/use-event-retargeting.html
      if (hit_test.action != kHitTestForeground)
        return false;
      return NGBoxFragmentPainter(fragment).NodeAtPoint(
          *hit_test.result, hit_test.location, physical_offset,
          kHitTestChildBlockBackgrounds);
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
    if (!is_in_atomic_painting_pass)
      return false;
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

  // Box fragments for SVG's inline boxes don't have correct geometries.
  if (!item.GetLayoutObject()->IsSVGInline()) {
    if (const NGPhysicalBoxFragment* child_fragment = item.BoxFragment()) {
      const PhysicalOffset child_offset =
          hit_test.inline_root_offset + item.OffsetInContainerFragment();
      return HitTestChildBoxFragment(hit_test, *child_fragment, cursor,
                                     child_offset);
    }
    DCHECK(
        !To<LayoutInline>(item.GetLayoutObject())->ShouldCreateBoxFragment());
  }

  DCHECK(item.GetLayoutObject()->IsLayoutInline());
  if (NGInlineCursor descendants = cursor.CursorForDescendants()) {
    if (HitTestItemsChildren(hit_test, container, descendants))
      return true;
  }

  if (cursor.ContainerFragment().IsSvgText()) {
    if (item.Style().UsedPointerEvents() != EPointerEvents::kBoundingBox)
      return false;
    // Now hit test ourselves.
    if (hit_test.action != kHitTestForeground ||
        !IsVisibleToHitTest(item, hit_test.result->GetHitTestRequest()))
      return false;
    // In SVG <text>, we should not refer to the geometry of kBox
    // NGFragmentItems because they don't have final values.
    auto bounds_rect = PhysicalRect::EnclosingRect(
        item.GetLayoutObject()->ObjectBoundingBox());
    return hit_test.location.Intersects(bounds_rect) &&
           hit_test.AddNodeToResultWithContentOffset(
               item.NodeForHitTest(), cursor.ContainerFragment(), bounds_rect,
               bounds_rect.offset);
  }

  // Now hit test ourselves.
  if (hit_test.action == kHitTestForeground &&
      IsVisibleToHitTest(item, hit_test.result->GetHitTestRequest())) {
    const PhysicalOffset child_offset =
        hit_test.inline_root_offset + item.OffsetInContainerFragment();
    PhysicalRect bounds_rect(child_offset, item.Size());
    if (UNLIKELY(
            hit_test.result->GetHitTestRequest().IsHitTestVisualOverflow())) {
      bounds_rect = item.SelfInkOverflow();
      bounds_rect.Move(child_offset);
    }
    // TODO(kojii): Don't have good explanation why only inline box needs to
    // snap, but matches to legacy and fixes crbug.com/976606.
    bounds_rect = PhysicalRect(ToPixelSnappedRect(bounds_rect));
    if (hit_test.location.Intersects(bounds_rect)) {
      if (hit_test.AddNodeToResultWithContentOffset(item.NodeForHitTest(),
                                                    cursor.ContainerFragment(),
                                                    bounds_rect, child_offset))
        return true;
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestChildren(
    const HitTestContext& hit_test,
    const PhysicalOffset& accumulated_offset) {
  if (UNLIKELY(inline_box_cursor_)) {
    NGInlineCursor descendants = inline_box_cursor_->CursorForDescendants();
    if (descendants) {
      return HitTestChildren(hit_test, PhysicalFragment(), descendants,
                             accumulated_offset);
    }
    return false;
  }
  if (items_) {
    const NGPhysicalBoxFragment& fragment = PhysicalFragment();
    NGInlineCursor cursor(fragment, *items_);
    return HitTestChildren(hit_test, fragment, cursor, accumulated_offset);
  }
  // Check descendants of this fragment because floats may be in the
  // |NGFragmentItems| of the descendants.
  if (hit_test.action == kHitTestFloat &&
      box_fragment_.HasFloatingDescendantsForPaint()) {
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
  if (children.HasRoot())
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
    if (UNLIKELY(block_child.IsLayoutObjectDestroyedOrMoved()))
      continue;
    if (block_child.HasSelfPaintingLayer() || block_child.IsFloating())
      continue;

    const PhysicalOffset child_offset = accumulated_offset + child.offset;

    if (block_child.IsPaintedAtomically()) {
      if (action != kHitTestForeground)
        continue;
      if (!HitTestAllPhasesInFragment(block_child, hit_test_location,
                                      child_offset, &result))
        continue;
    } else {
      if (!NodeAtPointInFragment(block_child, hit_test_location, child_offset,
                                 action, &result))
        continue;
    }

    if (result.InnerNode())
      return true;

    if (Node* node = block_child.NodeForHitTest()) {
      result.SetNodeAndPosition(node, &block_child,
                                hit_test_location.Point() - accumulated_offset);
      return true;
    }

    // Our child may have been an anonymous-block, update the hit-test node
    // to include our node if needed.
    Node* node = box_fragment_.NodeForHitTest();
    if (!node)
      return true;

    // Note: |accumulated_offset| includes container scrolled offset added
    // in |NGBoxFragmentPainter::NodeAtPoint()|. See http://crbug.com/1268782
    const PhysicalOffset scrolled_offset =
        box_fragment_.IsScrollContainer()
            ? PhysicalOffset(box_fragment_.PixelSnappedScrolledContentOffset())
            : PhysicalOffset();
    result.SetNodeAndPosition(
        node, &box_fragment_,
        hit_test_location.Point() - accumulated_offset - scrolled_offset);
    return true;
  }

  return false;
}

// static
bool NGBoxFragmentPainter::ShouldHitTestCulledInlineAncestors(
    const HitTestContext& hit_test,
    const NGFragmentItem& item) {
  if (hit_test.action != kHitTestForeground)
    return false;
  if (item.Type() == NGFragmentItem::kLine)
    return false;
  if (hit_test.result->GetHitTestRequest().ListBased()) {
    // For list base hit test, we should include culled inline into list.
    // DocumentOrShadowRoot-prototype-elementFromPoint.html requires this.
    return true;
  }
  if (item.IsBlockInInline()) {
    // To handle, empty size <div>, we skip hit testing on culled inline box.
    // See "fast/events/ondragenter.html".
    //
    // Culled inline should be handled by item in another line for block-in-
    // inline, e.g. <span>a<div>b</div></span>.
    return false;
  }
  return !item.IsBlockInInline();
}

bool NGBoxFragmentPainter::HitTestItemsChildren(
    const HitTestContext& hit_test,
    const NGPhysicalBoxFragment& container,
    const NGInlineCursor& children) {
  DCHECK(children.HasRoot());
  for (NGInlineBackwardCursor cursor(children); cursor;) {
    const NGFragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      // TODO(crbug.com/1099613): This should not happen, as long as it is
      // really layout-clean.
      NOTREACHED();
      cursor.MoveToPreviousSibling();
      continue;
    }

    if (item->HasSelfPaintingLayer()) {
      cursor.MoveToPreviousSibling();
      continue;
    }

    if (item->IsText()) {
      if (HitTestTextItem(hit_test, *item, cursor))
        return true;
    } else if (item->Type() == NGFragmentItem::kLine) {
      const NGPhysicalLineBoxFragment* child_fragment = item->LineBoxFragment();
      DCHECK(child_fragment);
      const PhysicalOffset child_offset =
          hit_test.inline_root_offset + item->OffsetInContainerFragment();
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

    if (ShouldHitTestCulledInlineAncestors(hit_test, *item)) {
      // Hit test culled inline boxes between |fragment| and its parent
      // fragment.
      const PhysicalOffset child_offset =
          hit_test.inline_root_offset + item->OffsetInContainerFragment();
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
    const NGPhysicalFragment& container,
    const PhysicalOffset& accumulated_offset) {
  DCHECK_EQ(hit_test.action, kHitTestFloat);
  DCHECK(container.HasFloatingDescendantsForPaint());

  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(&container)) {
    if (const NGFragmentItems* items = box->Items()) {
      NGInlineCursor children(*box, *items);
      if (HitTestFloatingChildItems(hit_test, children, accumulated_offset))
        return true;
      // Even if this turned out to be an inline formatting context, we need to
      // continue walking the box fragment children now. If a float is
      // block-fragmented, it is resumed as a regular box fragment child, rather
      // than becoming a fragment item.
    }
  }

  auto children = container.Children();
  for (const NGLink& child : base::Reversed(children)) {
    const NGPhysicalFragment& child_fragment = *child.fragment;
    if (UNLIKELY(child_fragment.IsLayoutObjectDestroyedOrMoved()))
      continue;
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

    // If this is a legacy root, fallback to legacy. It does not have
    // |HasFloatingDescendantsForPaint()| set, but it may have floating
    // descendants.
    if (child_fragment.IsLegacyLayoutRoot()) {
      if (child_fragment.GetMutableLayoutObject()->NodeAtPoint(
              *hit_test.result, hit_test.location, child_offset,
              hit_test.action))
        return true;
      continue;
    }
    if (!child_fragment.HasFloatingDescendantsForPaint())
      continue;

    if (child_fragment.HasNonVisibleOverflow()) {
      // We need to properly visit this fragment for hit-testing, rather than
      // jumping directly to its children (which is what we normally do when
      // looking for floats), in order to set up the clip rectangle.
      if (NodeAtPointInFragment(To<NGPhysicalBoxFragment>(child_fragment),
                                hit_test.location, child_offset, kHitTestFloat,
                                hit_test.result))
        return true;
      continue;
    }

    if (HitTestFloatingChildren(hit_test, child_fragment, child_offset))
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
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved()))
      continue;
    if (item->Type() == NGFragmentItem::kBox) {
      if (const NGPhysicalBoxFragment* child_box = item->BoxFragment()) {
        if (child_box->HasSelfPaintingLayer())
          continue;

        const PhysicalOffset child_offset =
            accumulated_offset + item->OffsetInContainerFragment();
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
  return !hit_test_location.Intersects(
      RoundedBorderGeometry::PixelSnappedRoundedBorder(
          style, rect, box_fragment_.SidesToInclude()));
}

bool NGBoxFragmentPainter::HitTestOverflowControl(
    const HitTestContext& hit_test,
    PhysicalOffset accumulated_offset) {
  const auto* layout_box =
      DynamicTo<LayoutBox>(box_fragment_.GetLayoutObject());
  return layout_box &&
         layout_box->HitTestOverflowControl(*hit_test.result, hit_test.location,
                                            accumulated_offset);
}

gfx::Rect NGBoxFragmentPainter::VisualRect(const PhysicalOffset& paint_offset) {
  if (const auto* layout_box =
          DynamicTo<LayoutBox>(box_fragment_.GetLayoutObject()))
    return BoxPainter(*layout_box).VisualRect(paint_offset);

  DCHECK(box_item_);
  PhysicalRect ink_overflow = box_item_->InkOverflow();
  ink_overflow.Move(paint_offset);
  return ToEnclosingRect(ink_overflow);
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LayoutBoxItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/Histogram.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

ScrollAnchor::ScrollAnchor()
    : anchor_object_(nullptr),
      corner_(Corner::kTopLeft),
      scroll_anchor_disabling_style_changed_(false),
      queued_(false) {}

ScrollAnchor::ScrollAnchor(ScrollableArea* scroller) : ScrollAnchor() {
  SetScroller(scroller);
}

ScrollAnchor::~ScrollAnchor() {}

void ScrollAnchor::SetScroller(ScrollableArea* scroller) {
  DCHECK_NE(scroller_, scroller);
  DCHECK(scroller);
  DCHECK(scroller->IsRootFrameViewport() || scroller->IsLocalFrameView() ||
         scroller->IsPaintLayerScrollableArea());
  scroller_ = scroller;
  ClearSelf();
}

// TODO(pilgrim): Replace all instances of scrollerLayoutBox with
// scrollerLayoutBoxItem, https://crbug.com/499321
static LayoutBox* ScrollerLayoutBox(const ScrollableArea* scroller) {
  LayoutBox* box = scroller->GetLayoutBox();
  DCHECK(box);
  return box;
}

static LayoutBoxItem ScrollerLayoutBoxItem(const ScrollableArea* scroller) {
  return LayoutBoxItem(ScrollerLayoutBox(scroller));
}

// TODO(skobes): Storing a "corner" doesn't make much sense anymore since we
// adjust only on the block flow axis.  This could probably be refactored to
// simply measure the movement of the block-start edge.
static Corner CornerToAnchor(const ScrollableArea* scroller) {
  const ComputedStyle* style = ScrollerLayoutBox(scroller)->Style();
  if (style->IsFlippedBlocksWritingMode())
    return Corner::kTopRight;
  return Corner::kTopLeft;
}

static LayoutPoint CornerPointOfRect(LayoutRect rect, Corner which_corner) {
  switch (which_corner) {
    case Corner::kTopLeft:
      return rect.MinXMinYCorner();
    case Corner::kTopRight:
      return rect.MaxXMinYCorner();
  }
  NOTREACHED();
  return LayoutPoint();
}

// Bounds of the LayoutObject relative to the scroller's visible content rect.
static LayoutRect RelativeBounds(const LayoutObject* layout_object,
                                 const ScrollableArea* scroller) {
  LayoutRect local_bounds;
  if (layout_object->IsBox()) {
    local_bounds = ToLayoutBox(layout_object)->BorderBoxRect();
    if (!layout_object->HasOverflowClip()) {
      // borderBoxRect doesn't include overflow content and floats.
      LayoutUnit max_y =
          std::max(local_bounds.MaxY(),
                   ToLayoutBox(layout_object)->LayoutOverflowRect().MaxY());
      if (layout_object->IsLayoutBlockFlow() &&
          ToLayoutBlockFlow(layout_object)->ContainsFloats()) {
        // Note that lowestFloatLogicalBottom doesn't include floating
        // grandchildren.
        max_y = std::max(
            max_y,
            ToLayoutBlockFlow(layout_object)->LowestFloatLogicalBottom());
      }
      local_bounds.ShiftMaxYEdgeTo(max_y);
    }
  } else if (layout_object->IsText()) {
    // TODO(skobes): Use first and last InlineTextBox only?
    for (InlineTextBox* box = ToLayoutText(layout_object)->FirstTextBox(); box;
         box = box->NextTextBox())
      local_bounds.Unite(box->FrameRect());
  } else {
    // Only LayoutBox and LayoutText are supported.
    NOTREACHED();
  }

  LayoutRect relative_bounds = LayoutRect(
      scroller
          ->LocalToVisibleContentQuad(FloatRect(local_bounds), layout_object)
          .BoundingBox());

  return relative_bounds;
}

static LayoutPoint ComputeRelativeOffset(const LayoutObject* layout_object,
                                         const ScrollableArea* scroller,
                                         Corner corner) {
  return CornerPointOfRect(RelativeBounds(layout_object, scroller), corner);
}

static bool CandidateMayMoveWithScroller(const LayoutObject* candidate,
                                         const ScrollableArea* scroller) {
  if (const ComputedStyle* style = candidate->Style()) {
    if (style->HasViewportConstrainedPosition() ||
        style->HasStickyConstrainedPosition())
      return false;
  }

  LayoutObject::AncestorSkipInfo skip_info(ScrollerLayoutBox(scroller));
  candidate->Container(&skip_info);
  return !skip_info.AncestorSkipped();
}

ScrollAnchor::ExamineResult ScrollAnchor::Examine(
    const LayoutObject* candidate) const {
  if (candidate == ScrollerLayoutBox(scroller_))
    return ExamineResult(kContinue);

  if (candidate->IsLayoutInline())
    return ExamineResult(kContinue);

  // Anonymous blocks are not in the DOM tree and it may be hard for
  // developers to reason about the anchor node.
  if (candidate->IsAnonymous())
    return ExamineResult(kContinue);

  if (!candidate->IsText() && !candidate->IsBox())
    return ExamineResult(kSkip);

  if (!CandidateMayMoveWithScroller(candidate, scroller_))
    return ExamineResult(kSkip);

  if (candidate->Style()->OverflowAnchor() == EOverflowAnchor::kNone)
    return ExamineResult(kSkip);

  LayoutRect candidate_rect = RelativeBounds(candidate, scroller_);
  LayoutRect visible_rect =
      ScrollerLayoutBoxItem(scroller_).OverflowClipRect(LayoutPoint());

  bool occupies_space =
      candidate_rect.Width() > 0 && candidate_rect.Height() > 0;
  if (occupies_space && visible_rect.Intersects(candidate_rect)) {
    return ExamineResult(
        visible_rect.Contains(candidate_rect) ? kReturn : kConstrain,
        CornerToAnchor(scroller_));
  } else {
    return ExamineResult(kSkip);
  }
}

void ScrollAnchor::FindAnchor() {
  TRACE_EVENT0("blink", "ScrollAnchor::findAnchor");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Layout.ScrollAnchor.TimeToFindAnchor");
  FindAnchorRecursive(ScrollerLayoutBox(scroller_));
}

bool ScrollAnchor::FindAnchorRecursive(LayoutObject* candidate) {
  ExamineResult result = Examine(candidate);
  if (result.viable) {
    anchor_object_ = candidate;
    corner_ = result.corner;
  }

  if (result.status == kReturn)
    return true;

  if (result.status == kSkip)
    return false;

  for (LayoutObject* child = candidate->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (FindAnchorRecursive(child))
      return true;
  }

  // Make a separate pass to catch positioned descendants with a static DOM
  // parent that we skipped over (crbug.com/692701).
  if (candidate->IsLayoutBlock()) {
    if (TrackedLayoutBoxListHashSet* positioned_descendants =
            ToLayoutBlock(candidate)->PositionedObjects()) {
      for (LayoutBox* descendant : *positioned_descendants) {
        if (descendant->Parent() != candidate) {
          if (FindAnchorRecursive(descendant))
            return true;
        }
      }
    }
  }

  if (result.status == kConstrain)
    return true;

  DCHECK_EQ(result.status, kContinue);
  return false;
}

bool ScrollAnchor::ComputeScrollAnchorDisablingStyleChanged() {
  LayoutObject* current = AnchorObject();
  if (!current)
    return false;

  LayoutObject* scroller_box = ScrollerLayoutBox(scroller_);
  while (true) {
    DCHECK(current);
    if (current->ScrollAnchorDisablingStyleChanged())
      return true;
    if (current == scroller_box)
      return false;
    current = current->Parent();
  }
}

void ScrollAnchor::NotifyBeforeLayout() {
  if (queued_) {
    scroll_anchor_disabling_style_changed_ |=
        ComputeScrollAnchorDisablingStyleChanged();
    return;
  }
  DCHECK(scroller_);
  ScrollOffset scroll_offset = scroller_->GetScrollOffset();
  float block_direction_scroll_offset =
      ScrollerLayoutBox(scroller_)->IsHorizontalWritingMode()
          ? scroll_offset.Height()
          : scroll_offset.Width();
  if (block_direction_scroll_offset == 0) {
    ClearSelf();
    return;
  }

  if (!anchor_object_) {
    FindAnchor();
    if (!anchor_object_)
      return;

    anchor_object_->SetIsScrollAnchorObject();
    saved_relative_offset_ =
        ComputeRelativeOffset(anchor_object_, scroller_, corner_);
  }

  scroll_anchor_disabling_style_changed_ =
      ComputeScrollAnchorDisablingStyleChanged();

  LocalFrameView* frame_view = ScrollerLayoutBox(scroller_)->GetFrameView();
  ScrollableArea* owning_scroller =
      scroller_->IsRootFrameViewport()
          ? &ToRootFrameViewport(scroller_)->LayoutViewport()
          : scroller_.Get();
  frame_view->EnqueueScrollAnchoringAdjustment(owning_scroller);
  queued_ = true;
}

IntSize ScrollAnchor::ComputeAdjustment() const {
  // The anchor node can report fractional positions, but it is DIP-snapped when
  // painting (crbug.com/610805), so we must round the offsets to determine the
  // visual delta. If we scroll by the delta in LayoutUnits, the snapping of the
  // anchor node may round differently from the snapping of the scroll position.
  // (For example, anchor moving from 2.4px -> 2.6px is really 2px -> 3px, so we
  // should scroll by 1px instead of 0.2px.) This is true regardless of whether
  // the ScrollableArea actually uses fractional scroll positions.
  IntSize delta = RoundedIntSize(ComputeRelativeOffset(anchor_object_,
                                                       scroller_, corner_)) -
                  RoundedIntSize(saved_relative_offset_);

  // Only adjust on the block layout axis.
  if (ScrollerLayoutBox(scroller_)->IsHorizontalWritingMode())
    delta.SetWidth(0);
  else
    delta.SetHeight(0);
  return delta;
}

void ScrollAnchor::Adjust() {
  if (!queued_)
    return;
  queued_ = false;
  DCHECK(scroller_);
  if (!anchor_object_)
    return;
  IntSize adjustment = ComputeAdjustment();
  if (adjustment.IsZero())
    return;

  if (scroll_anchor_disabling_style_changed_) {
    // Note that we only clear if the adjustment would have been non-zero.
    // This minimizes redundant calls to findAnchor.
    // TODO(skobes): add UMA metric for this.
    ClearSelf();

    DEFINE_STATIC_LOCAL(EnumerationHistogram, suppressed_by_sanaclap_histogram,
                        ("Layout.ScrollAnchor.SuppressedBySanaclap", 2));
    suppressed_by_sanaclap_histogram.Count(1);

    return;
  }

  scroller_->SetScrollOffset(
      scroller_->GetScrollOffset() + FloatSize(adjustment), kAnchoringScroll);

  // Update UMA metric.
  DEFINE_STATIC_LOCAL(EnumerationHistogram, adjusted_offset_histogram,
                      ("Layout.ScrollAnchor.AdjustedScrollOffset", 2));
  adjusted_offset_histogram.Count(1);
  UseCounter::Count(ScrollerLayoutBox(scroller_)->GetDocument(),
                    WebFeature::kScrollAnchored);
}

void ScrollAnchor::ClearSelf() {
  LayoutObject* anchor_object = anchor_object_;
  anchor_object_ = nullptr;

  if (anchor_object)
    anchor_object->MaybeClearIsScrollAnchorObject();
}

void ScrollAnchor::Clear() {
  LayoutObject* layout_object =
      anchor_object_ ? anchor_object_ : ScrollerLayoutBox(scroller_);
  PaintLayer* layer = nullptr;
  if (LayoutObject* parent = layout_object->Parent())
    layer = parent->EnclosingLayer();

  // Walk up the layer tree to clear any scroll anchors.
  while (layer) {
    if (PaintLayerScrollableArea* scrollable_area =
            layer->GetScrollableArea()) {
      ScrollAnchor* anchor = scrollable_area->GetScrollAnchor();
      DCHECK(anchor);
      anchor->ClearSelf();
    }
    layer = layer->Parent();
  }

  if (LocalFrameView* view = layout_object->GetFrameView()) {
    ScrollAnchor* anchor = view->GetScrollAnchor();
    DCHECK(anchor);
    anchor->ClearSelf();
  }
}

bool ScrollAnchor::RefersTo(const LayoutObject* layout_object) const {
  return anchor_object_ == layout_object;
}

void ScrollAnchor::NotifyRemoved(LayoutObject* layout_object) {
  if (anchor_object_ == layout_object)
    ClearSelf();
}

}  // namespace blink

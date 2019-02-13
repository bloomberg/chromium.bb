// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

namespace {

bool IsContainingBlockChainDescendant(LayoutObject* descendant,
                                      LayoutObject* ancestor) {
  LocalFrame* ancestor_frame = ancestor->GetDocument().GetFrame();
  LocalFrame* descendant_frame = descendant->GetDocument().GetFrame();
  if (ancestor_frame != descendant_frame)
    return false;

  while (descendant && descendant != ancestor)
    descendant = descendant->ContainingBlock();
  return descendant;
}

void MapRectUpToDocument(LayoutRect& rect,
                         const LayoutObject& descendant,
                         const Document& document) {
  FloatQuad mapped_quad = descendant.LocalToAncestorQuad(
      FloatQuad(FloatRect(rect)), document.GetLayoutView(),
      kUseTransforms | kApplyContainerFlip);
  rect = LayoutRect(mapped_quad.BoundingBox());
}

void MapRectDownToDocument(LayoutRect& rect,
                           LayoutBoxModelObject* ancestor,
                           const Document& document) {
  FloatQuad mapped_quad = document.GetLayoutView()->AncestorToLocalQuad(
      ancestor, FloatQuad(FloatRect(rect)),
      kUseTransforms | kApplyContainerFlip | kTraverseDocumentBoundaries);
  rect = LayoutRect(mapped_quad.BoundingBox());
}

LayoutUnit ComputeMargin(const Length& length, LayoutUnit reference_length) {
  if (length.IsPercent()) {
    return LayoutUnit(static_cast<int>(reference_length.ToFloat() *
                                       length.Percent() / 100.0));
  }
  DCHECK(length.IsFixed());
  return LayoutUnit(length.IntValue());
}

LayoutView* LocalRootView(Element& element) {
  LocalFrame* frame = element.GetDocument().GetFrame();
  LocalFrame* frame_root = frame ? &frame->LocalFrameRoot() : nullptr;
  return frame_root ? frame_root->ContentLayoutObject() : nullptr;
}

}  // namespace

IntersectionGeometry::IntersectionGeometry(Element* root_element,
                                           Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds,
                                           unsigned flags)
    : root_(root_element ? root_element->GetLayoutObject()
                         : LocalRootView(target_element)),
      target_(target_element.GetLayoutObject()),
      root_margin_(root_margin),
      thresholds_(thresholds),
      flags_(flags),
      intersection_ratio_(0),
      threshold_index_(0) {
  DCHECK(root_margin.IsEmpty() || root_margin.size() == 4);
  if (root_element)
    flags_ &= ~kRootIsImplicit;
  else
    flags_ |= kRootIsImplicit;
  flags_ &= ~kIsVisible;
  if (CanComputeGeometry(root_element, target_element))
    ComputeGeometry();
}

IntersectionGeometry::~IntersectionGeometry() = default;

bool IntersectionGeometry::CanComputeGeometry(Element* root,
                                              Element& target) const {
  if (root && !root->isConnected())
    return false;
  if (!root_ || !root_->IsBox())
    return false;
  if (!target.isConnected())
    return false;
  if (!target_ || (!target_->IsBoxModelObject() && !target_->IsText()))
    return false;
  if (root && !IsContainingBlockChainDescendant(target_, root_))
    return false;
  return true;
}

void IntersectionGeometry::InitializeTargetRect() {
  if (target_->IsBox()) {
    target_rect_ =
        LayoutRect(ToLayoutBoxModelObject(target_)->BorderBoundingBox());
  } else if (target_->IsLayoutInline()) {
    target_rect_ = ToLayoutInline(target_)->LinesBoundingBox();
  } else {
    target_rect_ = ToLayoutText(target_)->LinesBoundingBox();
  }
}

void IntersectionGeometry::InitializeRootRect() {
  if (root_->IsLayoutView() && root_->GetDocument().IsInMainFrame()) {
    // The main frame is a bit special as the scrolling viewport can differ in
    // size from the LayoutView itself. There's two situations this occurs in:
    // 1) The ForceZeroLayoutHeight quirk setting is used in Android WebView for
    // compatibility and sets the initial-containing-block's (a.k.a.
    // LayoutView) height to 0. Thus, we can't use its size for intersection
    // testing. Use the FrameView geometry instead.
    // 2) An element wider than the ICB can cause us to resize the FrameView so
    // we can zoom out to fit the entire element width.
    root_rect_ = ToLayoutView(root_)->OverflowClipRect(LayoutPoint());
  } else if (root_->IsBox() && root_->HasOverflowClip()) {
    root_rect_ = LayoutRect(ToLayoutBox(root_)->PhysicalContentBoxRect());
  } else {
    root_rect_ = LayoutRect(ToLayoutBoxModelObject(root_)->BorderBoundingBox());
  }
  ApplyRootMargin();
}

void IntersectionGeometry::ApplyRootMargin() {
  if (root_margin_.IsEmpty())
    return;

  // TODO(szager): Make sure the spec is clear that left/right margins are
  // resolved against width and not height.
  LayoutUnit top_margin = ComputeMargin(root_margin_[0], root_rect_.Height());
  LayoutUnit right_margin = ComputeMargin(root_margin_[1], root_rect_.Width());
  LayoutUnit bottom_margin =
      ComputeMargin(root_margin_[2], root_rect_.Height());
  LayoutUnit left_margin = ComputeMargin(root_margin_[3], root_rect_.Width());

  root_rect_.SetX(root_rect_.X() - left_margin);
  root_rect_.SetWidth(root_rect_.Width() + left_margin + right_margin);
  root_rect_.SetY(root_rect_.Y() - top_margin);
  root_rect_.SetHeight(root_rect_.Height() + top_margin + bottom_margin);
}

unsigned IntersectionGeometry::FirstThresholdGreaterThan(float ratio) const {
  unsigned result = 0;
  while (result < thresholds_.size() && thresholds_[result] <= ratio)
    ++result;
  return result;
}

bool IntersectionGeometry::ClipToRoot() {
  // Map and clip rect into root element coordinates.
  // TODO(szager): the writing mode flipping needs a test.
  LayoutBox* local_ancestor = nullptr;
  if (!RootIsImplicit() || root_->GetDocument().IsInMainFrame())
    local_ancestor = ToLayoutBox(root_);

  LayoutView* layout_view = target_->GetDocument().GetLayoutView();

  unsigned flags = kDefaultVisualRectFlags | kEdgeInclusive;
  if (!layout_view->NeedsPaintPropertyUpdate() &&
      !layout_view->DescendantNeedsPaintPropertyUpdate()) {
    flags |= kUseGeometryMapper;
  }
  bool does_intersect = target_->MapToVisualRectInAncestorSpace(
      local_ancestor, intersection_rect_, static_cast<VisualRectFlags>(flags));
  if (!does_intersect || !local_ancestor)
    return does_intersect;
  if (local_ancestor->HasOverflowClip())
    intersection_rect_.Move(-local_ancestor->ScrolledContentOffset());
  LayoutRect root_clip_rect(root_rect_);
  local_ancestor->FlipForWritingMode(root_clip_rect);
  return does_intersect & intersection_rect_.InclusiveIntersect(root_clip_rect);
}

void IntersectionGeometry::MapTargetRectToTargetFrameCoordinates() {
  Document& target_document = target_->GetDocument();
  MapRectUpToDocument(target_rect_, *target_, target_document);
}

void IntersectionGeometry::MapRootRectToRootFrameCoordinates() {
  root_rect_ = LayoutRect(
      root_
          ->LocalToAncestorQuad(
              FloatQuad(FloatRect(root_rect_)),
              RootIsImplicit() ? nullptr : root_->GetDocument().GetLayoutView(),
              kUseTransforms | kApplyContainerFlip)
          .BoundingBox());
}

void IntersectionGeometry::MapIntersectionRectToTargetFrameCoordinates() {
  Document& target_document = target_->GetDocument();
  if (RootIsImplicit()) {
    LocalFrame* target_frame = target_document.GetFrame();
    Frame& root_frame = target_frame->Tree().Top();
    if (target_frame != &root_frame)
      MapRectDownToDocument(intersection_rect_, nullptr, target_document);
  } else {
    MapRectUpToDocument(intersection_rect_, *root_, root_->GetDocument());
  }
}

void IntersectionGeometry::ComputeGeometry() {
  InitializeTargetRect();
  intersection_rect_ = target_rect_;
  InitializeRootRect();
  DCHECK(root_);
  DCHECK(target_);
  DCHECK(!target_->GetDocument().View()->NeedsLayout());
  bool does_intersect = ClipToRoot();
  MapTargetRectToTargetFrameCoordinates();
  if (does_intersect)
    MapIntersectionRectToTargetFrameCoordinates();
  else
    intersection_rect_ = LayoutRect();
  // Small optimization: if we're not going to report root bounds, don't bother
  // transforming them to the frame.
  if (ShouldReportRootBounds())
    MapRootRectToRootFrameCoordinates();

  // Some corner cases for threshold index:
  //   - If target rect is zero area, because it has zero width and/or zero
  //     height,
  //     only two states are recognized:
  //     - 0 means not intersecting.
  //     - 1 means intersecting.
  //     No other threshold crossings are possible.
  //   - Otherwise:
  //     - If root and target do not intersect, the threshold index is 0.

  //     - If root and target intersect but the intersection has zero-area
  //       (i.e., they have a coincident edge or corner), we consider the
  //       intersection to have "crossed" a zero threshold, but not crossed
  //       any non-zero threshold.

  if (does_intersect) {
    const LayoutRect comparison_rect =
        ShouldTrackFractionOfRoot() ? RootRect() : TargetRect();
    if (comparison_rect.IsEmpty()) {
      intersection_ratio_ = 1;
    } else {
      const LayoutSize& intersection_size = IntersectionRect().Size();
      const float intersection_area = intersection_size.Width().ToFloat() *
                                      intersection_size.Height().ToFloat();
      const LayoutSize& comparison_size = comparison_rect.Size();
      const float area_of_interest = comparison_size.Width().ToFloat() *
                                     comparison_size.Height().ToFloat();
      intersection_ratio_ = intersection_area / area_of_interest;
    }
    threshold_index_ = FirstThresholdGreaterThan(intersection_ratio_);
  } else {
    intersection_ratio_ = 0;
    threshold_index_ = 0;
  }
  ComputeVisibility();
}

void IntersectionGeometry::ComputeVisibility() {
  if (!IsIntersecting() || !ShouldComputeVisibility())
    return;
  DCHECK(RuntimeEnabledFeatures::IntersectionObserverV2Enabled());
  if (target_->GetDocument()
          .GetFrame()
          ->LocalFrameRoot()
          .MayBeOccludedOrObscuredByRemoteAncestor()) {
    return;
  }
  if (target_->HasDistortingVisualEffects())
    return;
  // TODO(layout-dev): This should hit-test the intersection rect, not the
  // target rect; it's not helpful to know that the portion of the target that
  // is clipped is also occluded. To do that, the intersection rect must be
  // mapped down to the local space of the target element.
  HitTestResult result(target_->HitTestForOcclusion(TargetRect()));
  if (!result.InnerNode() || result.InnerNode() == target_->GetNode())
    flags_ |= kIsVisible;
}

LayoutRect IntersectionGeometry::UnZoomedTargetRect() const {
  if (!target_)
    return target_rect_;
  FloatRect rect(target_rect_);
  AdjustForAbsoluteZoom::AdjustFloatRect(rect, *target_);
  return LayoutRect(rect);
}

LayoutRect IntersectionGeometry::UnZoomedIntersectionRect() const {
  if (!target_)
    return intersection_rect_;
  FloatRect rect(intersection_rect_);
  AdjustForAbsoluteZoom::AdjustFloatRect(rect, *target_);
  return LayoutRect(rect);
}

LayoutRect IntersectionGeometry::UnZoomedRootRect() const {
  if (!root_)
    return root_rect_;
  FloatRect rect(root_rect_);
  AdjustForAbsoluteZoom::AdjustFloatRect(rect, *root_);
  return LayoutRect(rect);
}

IntersectionObserverEntry* IntersectionGeometry::CreateEntry(
    Element* target,
    DOMHighResTimeStamp timestamp) {
  FloatRect root_bounds(UnZoomedRootRect());
  FloatRect* root_bounds_pointer =
      ShouldReportRootBounds() ? &root_bounds : nullptr;
  IntersectionObserverEntry* entry =
      MakeGarbageCollected<IntersectionObserverEntry>(
          timestamp, IntersectionRatio(), FloatRect(UnZoomedTargetRect()),
          root_bounds_pointer, FloatRect(UnZoomedIntersectionRect()),
          IsIntersecting(), IsVisible(), target);
  return entry;
}

}  // namespace blink

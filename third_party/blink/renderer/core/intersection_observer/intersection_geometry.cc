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
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
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

void MapRectUpToDocument(LayoutRect& rect, const LayoutObject& descendant) {
  FloatQuad mapped_quad =
      descendant.LocalToAncestorQuad(FloatQuad(FloatRect(rect)), nullptr,
                                     kUseTransforms | kApplyContainerFlip);
  rect = LayoutRect(mapped_quad.BoundingBox());
}

void MapRectDownToDocument(LayoutRect& rect,
                           const Document& document) {
  FloatQuad mapped_quad = document.GetLayoutView()->AncestorToLocalQuad(
      nullptr, FloatQuad(FloatRect(rect)),
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

bool ComputeIsVisible(LayoutObject* target, const LayoutRect& rect) {
  DCHECK(RuntimeEnabledFeatures::IntersectionObserverV2Enabled());
  if (target->GetDocument().GetFrame()->LocalFrameRoot().GetOcclusionState() !=
      kGuaranteedNotOccluded) {
    return false;
  }
  if (target->HasDistortingVisualEffects())
    return false;
  // TODO(layout-dev): This should hit-test the intersection rect, not the
  // target rect; it's not helpful to know that the portion of the target that
  // is clipped is also occluded.
  HitTestResult result(target->HitTestForOcclusion(rect));
  return (!result.InnerNode() || result.InnerNode() == target->GetNode());
}

static const unsigned kConstructorFlagsMask =
    IntersectionGeometry::kShouldReportRootBounds |
    IntersectionGeometry::kShouldComputeVisibility |
    IntersectionGeometry::kShouldTrackFractionOfRoot |
    IntersectionGeometry::kShouldUseReplacedContentRect |
    IntersectionGeometry::kShouldConvertToCSSPixels;

}  // namespace

IntersectionGeometry::IntersectionGeometry(Element* root_element,
                                           Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds,
                                           unsigned flags)
    : flags_(flags & kConstructorFlagsMask),
      intersection_ratio_(0),
      threshold_index_(0) {
  DCHECK(root_margin.IsEmpty() || root_margin.size() == 4);
  ComputeGeometry(root_element, target_element, root_margin, thresholds);
}

IntersectionGeometry::~IntersectionGeometry() = default;

void IntersectionGeometry::ComputeGeometry(Element* root_element,
                                           Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds) {
  LayoutObject* target = target_element.GetLayoutObject();
  LayoutObject* root;
  if (root_element) {
    root = root_element->GetLayoutObject();
    flags_ &= ~kRootIsImplicit;
  } else {
    root = LocalRootView(target_element);
    flags_ |= kRootIsImplicit;
  }
  if (!target_element.isConnected())
    return;
  if (root_element && !root_element->isConnected())
    return;
  if (!root || !root->IsBox())
    return;
  if (!target || (!target->IsBoxModelObject() && !target->IsText()))
    return;
  if (root_element && !IsContainingBlockChainDescendant(target, root))
    return;

  DCHECK(!target_element.GetDocument().View()->NeedsLayout());

  target_rect_ = InitializeTargetRect(target);
  intersection_rect_ = target_rect_;
  root_rect_ = InitializeRootRect(root, root_margin);
  bool does_intersect =
      ClipToRoot(root, target, root_rect_, intersection_rect_);
  MapRectUpToDocument(target_rect_, *target);
  if (does_intersect) {
    if (RootIsImplicit())
      MapRectDownToDocument(intersection_rect_, target->GetDocument());
    else
      MapRectUpToDocument(intersection_rect_, *root);
  } else {
    intersection_rect_ = LayoutRect();
  }
  MapRectUpToDocument(root_rect_, *root);

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
        ShouldTrackFractionOfRoot() ? root_rect_ : target_rect_;
    if (comparison_rect.IsEmpty()) {
      intersection_ratio_ = 1;
    } else {
      const LayoutSize& intersection_size = intersection_rect_.Size();
      const float intersection_area = intersection_size.Width().ToFloat() *
                                      intersection_size.Height().ToFloat();
      const LayoutSize& comparison_size = comparison_rect.Size();
      const float area_of_interest = comparison_size.Width().ToFloat() *
                                     comparison_size.Height().ToFloat();
      intersection_ratio_ = intersection_area / area_of_interest;
    }
    threshold_index_ =
        FirstThresholdGreaterThan(intersection_ratio_, thresholds);
  } else {
    intersection_ratio_ = 0;
    threshold_index_ = 0;
  }
  if (IsIntersecting() && ShouldComputeVisibility() &&
      ComputeIsVisible(target, target_rect_))
    flags_ |= kIsVisible;

  if (flags_ & kShouldConvertToCSSPixels) {
    FloatRect target_float_rect(target_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(target_float_rect, *target);
    target_rect_ = LayoutRect(target_float_rect);
    FloatRect intersection_float_rect(intersection_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(intersection_float_rect, *target);
    intersection_rect_ = LayoutRect(intersection_float_rect);
    FloatRect root_float_rect(root_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(root_float_rect, *root);
    root_rect_ = LayoutRect(root_float_rect);
  }
}

LayoutRect IntersectionGeometry::InitializeTargetRect(LayoutObject* target) {
  if ((flags_ & kShouldUseReplacedContentRect) &&
      target->IsLayoutEmbeddedContent()) {
    return ToLayoutEmbeddedContent(target)->ReplacedContentRect();
  }
  if (target->IsBox())
    return LayoutRect(ToLayoutBoxModelObject(target)->BorderBoundingBox());
  if (target->IsLayoutInline())
    return ToLayoutInline(target)->LinesBoundingBox();
  return ToLayoutText(target)->LinesBoundingBox();
}

LayoutRect IntersectionGeometry::InitializeRootRect(
    LayoutObject* root,
    const Vector<Length>& margin) {
  LayoutRect result;
  if (root->IsLayoutView() && root->GetDocument().IsInMainFrame()) {
    // The main frame is a bit special as the scrolling viewport can differ in
    // size from the LayoutView itself. There's two situations this occurs in:
    // 1) The ForceZeroLayoutHeight quirk setting is used in Android WebView for
    // compatibility and sets the initial-containing-block's (a.k.a.
    // LayoutView) height to 0. Thus, we can't use its size for intersection
    // testing. Use the FrameView geometry instead.
    // 2) An element wider than the ICB can cause us to resize the FrameView so
    // we can zoom out to fit the entire element width.
    result = ToLayoutView(root)->OverflowClipRect(LayoutPoint());
  } else if (root->IsBox() && root->HasOverflowClip()) {
    result = LayoutRect(ToLayoutBox(root)->PhysicalContentBoxRect());
  } else {
    result = LayoutRect(ToLayoutBoxModelObject(root)->BorderBoundingBox());
  }
  ApplyRootMargin(result, margin);
  return result;
}

void IntersectionGeometry::ApplyRootMargin(LayoutRect& rect,
                                           const Vector<Length>& margin) {
  if (margin.IsEmpty())
    return;

  // TODO(szager): Make sure the spec is clear that left/right margins are
  // resolved against width and not height.
  LayoutUnit top_margin = ComputeMargin(margin[0], rect.Height());
  LayoutUnit right_margin = ComputeMargin(margin[1], rect.Width());
  LayoutUnit bottom_margin = ComputeMargin(margin[2], rect.Height());
  LayoutUnit left_margin = ComputeMargin(margin[3], rect.Width());

  rect.SetX(rect.X() - left_margin);
  rect.SetWidth(rect.Width() + left_margin + right_margin);
  rect.SetY(rect.Y() - top_margin);
  rect.SetHeight(rect.Height() + top_margin + bottom_margin);
}

bool IntersectionGeometry::ClipToRoot(LayoutObject* root,
                                      LayoutObject* target,
                                      const LayoutRect& root_rect,
                                      LayoutRect& intersection_rect) {
  // Map and clip rect into root element coordinates.
  // TODO(szager): the writing mode flipping needs a test.
  LayoutBox* local_ancestor = nullptr;
  if (!RootIsImplicit() || root->GetDocument().IsInMainFrame())
    local_ancestor = ToLayoutBox(root);

  LayoutView* layout_view = target->GetDocument().GetLayoutView();

  unsigned flags = kDefaultVisualRectFlags | kEdgeInclusive;
  if (!layout_view->NeedsPaintPropertyUpdate() &&
      !layout_view->DescendantNeedsPaintPropertyUpdate()) {
    flags |= kUseGeometryMapper;
  }
  bool does_intersect = target->MapToVisualRectInAncestorSpace(
      local_ancestor, intersection_rect, static_cast<VisualRectFlags>(flags));
  if (!does_intersect || !local_ancestor)
    return does_intersect;
  if (local_ancestor->HasOverflowClip())
    intersection_rect.Move(-local_ancestor->ScrolledContentOffset());
  LayoutRect root_clip_rect(root_rect);
  local_ancestor->FlipForWritingMode(root_clip_rect);
  return does_intersect & intersection_rect.InclusiveIntersect(root_clip_rect);
}

unsigned IntersectionGeometry::FirstThresholdGreaterThan(
    float ratio,
    const Vector<float>& thresholds) const {
  unsigned result = 0;
  while (result < thresholds.size() && thresholds[result] <= ratio)
    ++result;
  return result;
}

}  // namespace blink

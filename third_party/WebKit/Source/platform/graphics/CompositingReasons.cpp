// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositingReasons.h"

#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

const CompositingReasonStringMap kCompositingReasonStringMap[] = {
    {kCompositingReasonNone, "Unknown", "No reason given"},
    {kCompositingReason3DTransform, "transform3D", "Has a 3d transform"},
    {kCompositingReasonVideo, "video", "Is an accelerated video"},
    {kCompositingReasonCanvas, "canvas",
     "Is an accelerated canvas, or is a display list backed canvas that was "
     "promoted to a layer based on a performance heuristic."},
    {kCompositingReasonPlugin, "plugin", "Is an accelerated plugin"},
    {kCompositingReasonIFrame, "iFrame", "Is an accelerated iFrame"},
    {kCompositingReasonBackfaceVisibilityHidden, "backfaceVisibilityHidden",
     "Has backface-visibility: hidden"},
    {kCompositingReasonRootScroller, "rootScroller",
     "Is the document.rootScroller"},
    {kCompositingReasonActiveAnimation, "activeAnimation",
     "Has an active accelerated animation or transition"},
    {kCompositingReasonTransitionProperty, "transitionProperty",
     "Has an acceleratable transition property (active or inactive)"},
    {kCompositingReasonScrollDependentPosition, "scrollDependentPosition",
     "Is fixed or sticky position"},
    {kCompositingReasonOverflowScrollingTouch, "overflowScrollingTouch",
     "Is a scrollable overflow element"},
    {kCompositingReasonOverflowScrollingParent, "overflowScrollingParent",
     "Scroll parent is not an ancestor"},
    {kCompositingReasonOutOfFlowClipping, "outOfFlowClipping",
     "Has clipping ancestor"},
    {kCompositingReasonVideoOverlay, "videoOverlay",
     "Is overlay controls for video"},
    {kCompositingReasonWillChangeCompositingHint, "willChange",
     "Has a will-change compositing hint"},
    {kCompositingReasonBackdropFilter, "backdropFilter",
     "Has a backdrop filter"},
    {kCompositingReasonCompositorProxy, "compositorProxy",
     "Has a CompositorProxy object"},
    {kCompositingReasonAssumedOverlap, "assumedOverlap",
     "Might overlap other composited content"},
    {kCompositingReasonOverlap, "overlap", "Overlaps other composited content"},
    {kCompositingReasonNegativeZIndexChildren, "negativeZIndexChildren",
     "Parent with composited negative z-index content"},
    {kCompositingReasonSquashingDisallowed, "squashingDisallowed",
     "Layer was separately composited because it could not be squashed."},
    {kCompositingReasonTransformWithCompositedDescendants,
     "transformWithCompositedDescendants",
     "Has a transform that needs to be known by compositor because of "
     "composited descendants"},
    {kCompositingReasonOpacityWithCompositedDescendants,
     "opacityWithCompositedDescendants",
     "Has opacity that needs to be applied by compositor because of composited "
     "descendants"},
    {kCompositingReasonMaskWithCompositedDescendants,
     "maskWithCompositedDescendants",
     "Has a mask that needs to be known by compositor because of composited "
     "descendants"},
    {kCompositingReasonReflectionWithCompositedDescendants,
     "reflectionWithCompositedDescendants",
     "Has a reflection that needs to be known by compositor because of "
     "composited descendants"},
    {kCompositingReasonFilterWithCompositedDescendants,
     "filterWithCompositedDescendants",
     "Has a filter effect that needs to be known by compositor because of "
     "composited descendants"},
    {kCompositingReasonBlendingWithCompositedDescendants,
     "blendingWithCompositedDescendants",
     "Has a blending effect that needs to be known by compositor because of "
     "composited descendants"},
    {kCompositingReasonClipsCompositingDescendants,
     "clipsCompositingDescendants",
     "Has a clip that needs to be known by compositor because of composited "
     "descendants"},
    {kCompositingReasonPerspectiveWith3DDescendants,
     "perspectiveWith3DDescendants",
     "Has a perspective transform that needs to be known by compositor because "
     "of 3d descendants"},
    {kCompositingReasonPreserve3DWith3DDescendants,
     "preserve3DWith3DDescendants",
     "Has a preserves-3d property that needs to be known by compositor because "
     "of 3d descendants"},
    {kCompositingReasonReflectionOfCompositedParent,
     "reflectionOfCompositedParent", "Is a reflection of a composited layer"},
    {kCompositingReasonIsolateCompositedDescendants,
     "isolateCompositedDescendants",
     "Should isolate descendants to apply a blend effect"},
    {kCompositingReasonPositionFixedOrStickyWithCompositedDescendants,
     "positionFixedOrStickyWithCompositedDescendants"
     "Is a position:fixed or position:sticky element with composited "
     "descendants"},
    {kCompositingReasonRoot, "root", "Is the root layer"},
    {kCompositingReasonLayerForAncestorClip, "layerForAncestorClip",
     "Secondary layer, applies a clip due to a sibling in the compositing "
     "tree"},
    {kCompositingReasonLayerForDescendantClip, "layerForDescendantClip",
     "Secondary layer, to clip descendants of the owning layer"},
    {kCompositingReasonLayerForPerspective, "layerForPerspective",
     "Secondary layer, to house the perspective transform for all descendants"},
    {kCompositingReasonLayerForHorizontalScrollbar,
     "layerForHorizontalScrollbar",
     "Secondary layer, the horizontal scrollbar layer"},
    {kCompositingReasonLayerForVerticalScrollbar, "layerForVerticalScrollbar",
     "Secondary layer, the vertical scrollbar layer"},
    {kCompositingReasonLayerForOverflowControlsHost,
     "layerForOverflowControlsHost",
     "Secondary layer, the overflow controls host layer"},
    {kCompositingReasonLayerForScrollCorner, "layerForScrollCorner",
     "Secondary layer, the scroll corner layer"},
    {kCompositingReasonLayerForScrollingContents, "layerForScrollingContents",
     "Secondary layer, to house contents that can be scrolled"},
    {kCompositingReasonLayerForScrollingContainer, "layerForScrollingContainer",
     "Secondary layer, used to position the scrolling contents while "
     "scrolling"},
    {kCompositingReasonLayerForSquashingContents, "layerForSquashingContents",
     "Secondary layer, home for a group of squashable content"},
    {kCompositingReasonLayerForSquashingContainer, "layerForSquashingContainer",
     "Secondary layer, no-op layer to place the squashing layer correctly in "
     "the composited layer tree"},
    {kCompositingReasonLayerForForeground, "layerForForeground",
     "Secondary layer, to contain any normal flow and positive z-index "
     "contents on top of a negative z-index layer"},
    {kCompositingReasonLayerForBackground, "layerForBackground",
     "Secondary layer, to contain acceleratable background content"},
    {kCompositingReasonLayerForMask, "layerForMask",
     "Secondary layer, to contain the mask contents"},
    {kCompositingReasonLayerForClippingMask, "layerForClippingMask",
     "Secondary layer, for clipping mask"},
    {kCompositingReasonLayerForAncestorClippingMask,
     "layerForAncestorClippingMask",
     "Secondary layer, applies a clipping mask due to a sibling in the "
     "composited layer tree"},
    {kCompositingReasonLayerForScrollingBlockSelection,
     "layerForScrollingBlockSelection",
     "Secondary layer, to house block selection gaps for composited scrolling "
     "with no scrolling contents"},
    {kCompositingReasonLayerForDecoration, "layerForDecoration",
     "Layer painted on top of other layers as decoration"},
    {kCompositingReasonInlineTransform, "inlineTransform",
     "Has an inline transform, which causes subsequent layers to assume "
     "overlap"},
};

const size_t kNumberOfCompositingReasons =
    WTF_ARRAY_LENGTH(kCompositingReasonStringMap);

String CompositingReasonsAsString(CompositingReasons reasons) {
  if (!reasons)
    return "none";

  StringBuilder builder;
  for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
    if (reasons & kCompositingReasonStringMap[i].reason) {
      if (builder.length())
        builder.Append(',');
      builder.Append(kCompositingReasonStringMap[i].short_name);
    }
  }
  return builder.ToString();
}

}  // namespace blink

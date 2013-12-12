// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/port.h"
#include "cc/layers/compositing_reasons.h"
#include "third_party/WebKit/public/platform/WebCompositingReasons.h"

#define COMPILE_ASSERT_MATCHING_UINT64(cc_name, webkit_name)                  \
    COMPILE_ASSERT(cc_name == webkit_name, mismatching_uint64)

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonUnknown,
    blink::CompositingReasonUnknown);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReason3DTransform,
    blink::CompositingReason3DTransform);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonVideo,
    blink::CompositingReasonVideo);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonCanvas,
    blink::CompositingReasonCanvas);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPlugin,
    blink::CompositingReasonPlugin);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonIFrame,
    blink::CompositingReasonIFrame);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonBackfaceVisibilityHidden,
    blink::CompositingReasonBackfaceVisibilityHidden);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonAnimation,
    blink::CompositingReasonAnimation);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonFilters,
    blink::CompositingReasonFilters);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPositionFixed,
    blink::CompositingReasonPositionFixed);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPositionSticky,
    blink::CompositingReasonPositionSticky);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOverflowScrollingTouch,
    blink::CompositingReasonOverflowScrollingTouch);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonAssumedOverlap,
    blink::CompositingReasonAssumedOverlap);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOverlap,
    blink::CompositingReasonOverlap);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonNegativeZIndexChildren,
    blink::CompositingReasonNegativeZIndexChildren);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonTransformWithCompositedDescendants,
    blink::CompositingReasonTransformWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOpacityWithCompositedDescendants,
    blink::CompositingReasonOpacityWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonMaskWithCompositedDescendants,
    blink::CompositingReasonMaskWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonReflectionWithCompositedDescendants,
    blink::CompositingReasonReflectionWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonFilterWithCompositedDescendants,
    blink::CompositingReasonFilterWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonBlendingWithCompositedDescendants,
    blink::CompositingReasonBlendingWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonClipsCompositingDescendants,
    blink::CompositingReasonClipsCompositingDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPerspective,
    blink::CompositingReasonPerspective);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPreserve3D,
    blink::CompositingReasonPreserve3D);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonReflectionOfCompositedParent,
    blink::CompositingReasonReflectionOfCompositedParent);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonRoot,
    blink::CompositingReasonRoot);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForClip,
    blink::CompositingReasonLayerForClip);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForScrollbar,
    blink::CompositingReasonLayerForScrollbar);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForScrollingContainer,
    blink::CompositingReasonLayerForScrollingContainer);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForForeground,
    blink::CompositingReasonLayerForForeground);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForBackground,
    blink::CompositingReasonLayerForBackground);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForMask,
    blink::CompositingReasonLayerForMask);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOverflowScrollingParent,
    blink::CompositingReasonOverflowScrollingParent);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOutOfFlowClipping,
    blink::CompositingReasonOutOfFlowClipping);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonIsolateCompositedDescendants,
    blink::CompositingReasonIsolateCompositedDescendants);

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
    WebKit::CompositingReasonUnknown);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReason3DTransform,
    WebKit::CompositingReason3DTransform);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonVideo,
    WebKit::CompositingReasonVideo);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonCanvas,
    WebKit::CompositingReasonCanvas);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPlugin,
    WebKit::CompositingReasonPlugin);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonIFrame,
    WebKit::CompositingReasonIFrame);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonBackfaceVisibilityHidden,
    WebKit::CompositingReasonBackfaceVisibilityHidden);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonAnimation,
    WebKit::CompositingReasonAnimation);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonFilters,
    WebKit::CompositingReasonFilters);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPositionFixed,
    WebKit::CompositingReasonPositionFixed);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPositionSticky,
    WebKit::CompositingReasonPositionSticky);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOverflowScrollingTouch,
    WebKit::CompositingReasonOverflowScrollingTouch);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonBlending,
    WebKit::CompositingReasonBlending);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonAssumedOverlap,
    WebKit::CompositingReasonAssumedOverlap);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOverlap,
    WebKit::CompositingReasonOverlap);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonNegativeZIndexChildren,
    WebKit::CompositingReasonNegativeZIndexChildren);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonTransformWithCompositedDescendants,
    WebKit::CompositingReasonTransformWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonOpacityWithCompositedDescendants,
    WebKit::CompositingReasonOpacityWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonMaskWithCompositedDescendants,
    WebKit::CompositingReasonMaskWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonReflectionWithCompositedDescendants,
    WebKit::CompositingReasonReflectionWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonFilterWithCompositedDescendants,
    WebKit::CompositingReasonFilterWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonBlendingWithCompositedDescendants,
    WebKit::CompositingReasonBlendingWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonClipsCompositingDescendants,
    WebKit::CompositingReasonClipsCompositingDescendants);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPerspective,
    WebKit::CompositingReasonPerspective);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonPreserve3D,
    WebKit::CompositingReasonPreserve3D);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonReflectionOfCompositedParent,
    WebKit::CompositingReasonReflectionOfCompositedParent);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonRoot,
    WebKit::CompositingReasonRoot);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForClip,
    WebKit::CompositingReasonLayerForClip);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForScrollbar,
    WebKit::CompositingReasonLayerForScrollbar);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForScrollingContainer,
    WebKit::CompositingReasonLayerForScrollingContainer);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForForeground,
    WebKit::CompositingReasonLayerForForeground);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForBackground,
    WebKit::CompositingReasonLayerForBackground);

COMPILE_ASSERT_MATCHING_UINT64(
    cc::kCompositingReasonLayerForMask,
    WebKit::CompositingReasonLayerForMask);

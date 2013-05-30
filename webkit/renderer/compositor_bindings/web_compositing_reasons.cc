// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "cc/layers/compositing_reasons.h"
#include "third_party/WebKit/public/platform/WebCompositingReasons.h"

#define COMPILE_ASSERT_MATCHING_ENUMS(cc_name, webkit_name)             \
    COMPILE_ASSERT(                                                     \
        static_cast<int>(cc_name) == static_cast<int>(webkit_name),     \
        mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonUnknown,
    WebKit::CompositingReasonUnknown);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReason3DTransform,
    WebKit::CompositingReason3DTransform);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonVideo,
    WebKit::CompositingReasonVideo);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonCanvas,
    WebKit::CompositingReasonCanvas);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonPlugin,
    WebKit::CompositingReasonPlugin);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonIFrame,
    WebKit::CompositingReasonIFrame);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonBackfaceVisibilityHidden,
    WebKit::CompositingReasonBackfaceVisibilityHidden);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonAnimation,
    WebKit::CompositingReasonAnimation);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonFilters,
    WebKit::CompositingReasonFilters);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonPositionFixed,
    WebKit::CompositingReasonPositionFixed);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonPositionSticky,
    WebKit::CompositingReasonPositionSticky);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonOverflowScrollingTouch,
    WebKit::CompositingReasonOverflowScrollingTouch);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonBlending,
    WebKit::CompositingReasonBlending);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonAssumedOverlap,
    WebKit::CompositingReasonAssumedOverlap);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonOverlap,
    WebKit::CompositingReasonOverlap);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonNegativeZIndexChildren,
    WebKit::CompositingReasonNegativeZIndexChildren);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonTransformWithCompositedDescendants,
    WebKit::CompositingReasonTransformWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonOpacityWithCompositedDescendants,
    WebKit::CompositingReasonOpacityWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonMaskWithCompositedDescendants,
    WebKit::CompositingReasonMaskWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonReflectionWithCompositedDescendants,
    WebKit::CompositingReasonReflectionWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonFilterWithCompositedDescendants,
    WebKit::CompositingReasonFilterWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonBlendingWithCompositedDescendants,
    WebKit::CompositingReasonBlendingWithCompositedDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonClipsCompositingDescendants,
    WebKit::CompositingReasonClipsCompositingDescendants);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonPerspective,
    WebKit::CompositingReasonPerspective);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonPreserve3D,
    WebKit::CompositingReasonPreserve3D);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonReflectionOfCompositedParent,
    WebKit::CompositingReasonReflectionOfCompositedParent);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonRoot,
    WebKit::CompositingReasonRoot);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonLayerForClip,
    WebKit::CompositingReasonLayerForClip);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonLayerForScrollbar,
    WebKit::CompositingReasonLayerForScrollbar);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonLayerForScrollingContainer,
    WebKit::CompositingReasonLayerForScrollingContainer);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonLayerForForeground,
    WebKit::CompositingReasonLayerForForeground);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonLayerForBackground,
    WebKit::CompositingReasonLayerForBackground);

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::kCompositingReasonLayerForMask,
    WebKit::CompositingReasonLayerForMask);
